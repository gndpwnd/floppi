# Camera Extrinsic Calibration for Aerial Robotics

## Table of Contents
1. [Extrinsic Parameters Definition](#extrinsic-parameters-definition)
2. [Mathematical Framework](#mathematical-framework)
3. [Transformation Chain](#transformation-chain)
4. [Camera Pose in World Frame](#camera-pose-in-world-frame)
5. [3D Reconstruction](#3d-reconstruction)
6. [Use Cases in Autonomous Drones](#use-cases-in-autonomous-drones)
7. [Calibration Methods](#calibration-methods)
8. [Practical Implementation](#practical-implementation)

---

## Extrinsic Parameters Definition

### Overview
Extrinsic parameters define the **rigid body transformation** between the camera coordinate system and an external reference frame (typically the drone body frame or world frame). Unlike intrinsic parameters (which describe the camera's internal properties like focal length), extrinsics define **where and how** the camera is positioned relative to the drone.

### Mathematical Components

Extrinsic parameters consist of two components:

#### 1. **Rotation Matrix (R)**: 3×3 Orthonormal Matrix
```
R = [r11  r12  r13]
    [r21  r22  r23]
    [r31  r32  r33]
```

- Describes the orientation of the camera frame relative to the body/world frame
- **Orthonormal**: R^T × R = I (transpose equals inverse)
- **Determinant**: det(R) = +1 (no scaling)
- Can be represented as:
  - Euler angles (roll, pitch, yaw)
  - Quaternions (4 parameters, more efficient for optimization)
  - Axis-angle representation
  - Rotation vector (3 parameters, used in OpenCV)

#### 2. **Translation Vector (t)**: 3×1 Vector
```
t = [tx]
    [ty]
    [tz]
```

- Represents the **position of the camera optical center** in the body/world frame
- Units typically in meters
- Can be thought of as the 3D coordinates of the camera origin in the reference frame

### Representation Variants

**Important distinction**: The translation vector has two common interpretations:

- **t (Camera Position)**: Position of camera optical center in world/body coordinates
  - Used for: projecting world points into camera view
  - Transformation: P_camera = R × P_world + t

- **t' (Inverse Translation)**: Position of world/body origin in camera coordinates  
  - Used in some formulations: P_camera = R × (P_world - C_world)
  - Where C_world is the camera center position
  - t' = -R^T × t (related by this transform)

---

## Mathematical Framework

### The Extrinsic Transformation Matrix

Extrinsics are often combined into a single **4×4 homogeneous transformation matrix** (SE(3) - Special Euclidean group):

```
T_world_to_camera = [R  t] = [r11  r12  r13  tx]
                    [0  1]   [r21  r22  r23  ty]
                             [r31  r32  r33  tz]
                             [0    0    0    1 ]
```

This matrix:
- Transforms points from world coordinates to camera coordinates
- Is a member of SE(3), the group of rigid body transformations
- Has an inverse: T^(-1) = [R^T  -R^T × t]
                             [0      1      ]

### Point Transformation

**Forward transformation** (world → camera):
```
P_camera = R × P_world + t
```
Or in homogeneous coordinates:
```
[P_camera]   [R  t] [P_world]
[   1   ] = [0  1] [  1    ]
```

**Inverse transformation** (camera → world):
```
P_world = R^T × (P_camera - t)
        = R^T × P_camera - R^T × t
```

### Composition of Transformations

When chaining multiple frames, transformation composition follows:
```
T_A_to_C = T_A_to_B × T_B_to_C
```

This is **right-multiplication order** for transformation composition, allowing us to "cancel out" intermediate frames.

---

## Transformation Chain

### The Complete Pipeline: Pixel → World Frame

Converting from 2D pixel coordinates to 3D world frame requires **four transformations**:

```
Pixel Coordinates → Camera Frame → Body Frame → World Frame
       (u,v)          (x_c, y_c, z_c)    (x_b, y_b, z_b)  (x_w, y_w, z_w)
```

### Step 1: Pixel → Camera Frame (Intrinsic Parameters)

Using the **camera calibration matrix (intrinsics)**:

```
       [fx  0  cx]
K =    [0  fy  cy]
       [0   0   1]
```

For a 3D point in camera frame [X_c, Y_c, Z_c]:

```
[u]     [X_c]
[v] = K[Y_c]  / Z_c
[1]     [Z_c]

Where:
- (u, v) = pixel coordinates
- fx, fy = focal lengths in pixels
- cx, cy = principal point (image center)
- Z_c = depth from camera
```

**Inverse (pixel depth → camera frame)**:
```
X_c = (u - cx) × Z_c / fx
Y_c = (v - cy) × Z_c / fy
```

### Step 2: Camera Frame → Body Frame (Camera Extrinsics)

This is the **primary concern** of this document. Using camera-to-body extrinsics:

```
T_body_to_camera = [R_c_b  t_c_b]
                   [0      1    ]

Where:
- R_c_b = 3×3 rotation from body to camera frame
- t_c_b = 3×1 position of camera optical center in body frame
```

Transform point from camera to body frame:
```
P_body = R_c_b^T × (P_camera - t_c_b)
```

Or equivalently, using the inverse transformation:
```
T_camera_to_body = T_body_to_camera^(-1)
P_body = R_c_b^T × P_camera - R_c_b^T × t_c_b
```

### Step 3: Body Frame → World Frame (Drone Pose)

The drone's pose at time t is given by:
- **Position**: P_body_in_world = [x_w, y_w, z_w]
- **Orientation**: R_world_to_body (from IMU or pose estimator)

```
T_world_to_body = [R_world_to_body  P_body_in_world]
                  [0                1              ]
```

Transform from body to world:
```
P_world = R_world_to_body^T × P_body + P_body_in_world
```

### Complete Transformation Chain

**Combined transformation**:
```
P_world = R_world_to_body^T × (R_c_b^T × (P_camera - t_c_b)) + P_body_in_world

Simplified:
P_world = R_world_to_body^T × R_c_b^T × P_camera 
        - R_world_to_body^T × R_c_b^T × t_c_b 
        + P_body_in_world
```

**Matrix form**:
```
T_world_to_camera = T_world_to_body × T_body_to_camera

[P_world]   [R_w_b  P_w] [R_c_b  t_c_b] [P_camera]
[  1    ] = [0      1  ] [0      1    ] [  1     ]
```

### Visual Representation

```
World Frame (fixed)
    ↑ z_w
    |     ← T_world_to_body (drone pose from IMU/GNSS)
    +────→ x_w
   /
  y_w

        Drone Body Frame (moving with drone)
            ↑ z_b
            |     ← T_body_to_camera (extrinsics - fixed)
            +────→ x_b
           /
          y_b
        
                Camera Frame (optical center origin)
                    → optical axis (z_c)
                   /|
                  / | ← focal length f
                 /  |
                /   ↓ image plane
```

---

## Camera Pose in World Frame

### Computing Camera Position in World

Given:
- Drone position in world: **P_drone = [x_w, y_w, z_w]^T**
- Drone orientation: **R_w_b** (rotation from body to world)
- Camera position in body frame: **t_c_b** (extrinsics)
- Camera orientation in body: **R_c_b** (extrinsics)

**Camera position in world**:
```
P_camera_world = R_w_b^T × t_c_b + P_drone

Or equivalently:
P_camera_world = R_w_b × t_c_b + P_drone
```

### Computing Camera Orientation in World

**Camera rotation in world**:
```
R_camera_world = R_w_b × R_c_b
```

This gives the camera's orientation relative to world axes.

### Practical Example

**Scenario**: Downward-looking camera (commonly used for visual odometry and precision landing)

```
Typical camera extrinsics for downward camera:
- Camera offset in body frame: t_c_b = [0, 0, -0.05]  (5cm below drone body)
- Camera rotation in body frame: R_c_b = 180° rotation around x-axis
  (so +z in camera frame points downward in body frame)

R_c_b ≈ [1   0    0  ]
        [0  -1    0  ]
        [0   0   -1  ]

If drone is at height h with level orientation (R_w_b = I):
- Camera world position: P_cam_world = [0, 0, -0.05] + [x_d, y_d, h]
                                      = [x_d, y_d, h - 0.05]
```

### Extracting Euler Angles from Camera Pose

Once you have R_camera_world, extract Euler angles (roll φ, pitch θ, yaw ψ):

```
# Z-Y-X Euler angles (common convention)
roll  = atan2(R[2,1], R[2,2])
pitch = asin(-R[2,0])
yaw   = atan2(R[1,0], R[0,0])
```

---

## 3D Reconstruction

### Principle: From Pixel → 3D World Point

With calibrated camera parameters and extrinsics, you can reconstruct 3D points from image coordinates.

### Requirements

1. **Intrinsic calibration**: K matrix (focal length, principal point, distortion)
2. **Extrinsic calibration**: R and t from camera to body/world
3. **Depth information**: 
   - From stereo matching
   - From depth sensor (LiDAR, RGB-D camera)
   - From structure-from-motion (multiple views)
   - From monocular depth estimation (neural network)

### Single Image 3D Reconstruction (with depth)

Given a pixel (u, v) and its depth d from a depth sensor:

**Step 1**: Pixel → Camera frame (undistort pixel, apply inverse intrinsics)
```
X_c = (u - cx) × d / fx
Y_c = (v - cy) × d / fy
Z_c = d

P_camera = [X_c, Y_c, Z_c]^T
```

**Step 2**: Camera → Body frame (using extrinsics)
```
P_body = R_c_b^T × (P_camera - t_c_b)
```

**Step 3**: Body → World frame (using drone pose)
```
P_world = R_w_b^T × P_body + P_drone_world
```

### Two-View Reconstruction (Stereo / Structure from Motion)

When depth is unknown but you have two views of the same scene:

#### Epipolar Geometry Constraints

**Essential Matrix** (E = [t]_× × R where [t]_× is skew-symmetric cross product):
```
E = [R_w_b1 × t_w_b1]_× × R_w_b1^T × R_w_b2
```

For a point visible in both images with corresponding pixel locations (u1, v1) and (u2, v2):

**Epipolar constraint**:
```
p2^T × E × p1 = 0

Where:
- p1 = [u1, v1, 1]^T (in camera 1 frame)
- p2 = [u2, v2, 1]^T (in camera 2 frame)
```

#### Triangulation (Computing Depth)

Using the **normalized epipolar constraint** from two views:

**Given**:
- Extrinsics: R and t between camera 1 and camera 2
- Projection matrices: P1 = K[I|0] and P2 = K[R|t]
- Corresponding points: p1, p2

**Triangulation**:
```
Minimize: ||x1 - P1 × X||^2 + ||x2 - P2 × X||^2

Where X is the unknown 3D point in camera 1 frame.

Solution: Use linear algebra (SVD) or nonlinear optimization
```

#### Triangulation for Moving Drone

For a moving drone observing a static scene point:

```
Frame 1: Drone at pose (R_1, t_1), sees point at pixel (u1, v1)
Frame 2: Drone at pose (R_2, t_2), sees same point at pixel (u2, v2)

Using chain rule:
T_camera1_to_camera2 = T_camera1_to_world × T_world_to_camera2

Then apply standard triangulation with computed relative pose.
```

### Keypoint-Based Structure from Motion

**Workflow for 3D scene reconstruction**:

1. **Feature Detection**: Find distinctive keypoints in image (SIFT, ORB, AKAZE)
2. **Feature Matching**: Match keypoints across image sequence
3. **Motion Estimation**: Compute relative camera poses from matched points
   - Use 5-point algorithm (when intrinsics known)
   - Use 8-point algorithm (when intrinsics unknown)
4. **Triangulation**: Compute 3D positions of matched keypoints
5. **Bundle Adjustment**: Refine camera poses and point positions jointly
6. **Transforming to World**: Apply extrinsic calibration to get world coordinates

---

## Use Cases in Autonomous Drones

### 1. Visual Odometry (VO)

**Purpose**: Estimate drone motion (position and orientation change) from camera images alone.

**Why Extrinsics Matter**:
- Camera motion must be transformed to body frame motion
- Integration of velocities requires proper reference frame
- Drift accumulates if transformation is inaccurate

**Workflow**:
```
Feature matching → Compute relative camera poses → 
Transform to body frame (using extrinsics) → 
Integrate to estimate drone trajectory
```

### 2. Structure from Motion (SfM)

**Purpose**: Build 3D maps of environment from image sequence.

**Applications**:
- Aerial surveying and mapping
- Photogrammetry
- Environmental monitoring
- Infrastructure inspection

**Why Extrinsics Matter**:
- All 3D points must be reconstructed in world frame
- Camera poses must be expressed relative to drone body
- Accurate extrinsics reduce reconstruction error

**Example Workflow**:
```
Capture image sequence → Detect/match features → 
Estimate relative camera poses → 
Transform to world using extrinsics and drone pose → 
Triangulate all feature points → 
Generate point cloud and mesh
```

### 3. Visual-Inertial Odometry (VIO)

**Purpose**: Fuse camera and IMU data for robust motion estimation.

**Why Extrinsics Matter**:
- **Camera-IMU extrinsics**: Rotation R_imu_to_cam and translation t_imu_to_cam
- Pre-integration requires proper frame alignment
- IMU bias estimation depends on camera-IMU alignment

**Typical System Flow**:
```
IMU data (gyro, accel) → Pre-integration with extrinsics →
Camera keyframes → Visual measurement → 
Sensor fusion (EKF/graph-based) → 
Drone pose (position + velocity + orientation)
```

**Ground Truth Validation**:
- Use calibrated extrinsics to compare IMU-integrated pose with visual pose
- Large divergence indicates calibration error or sensor bias

### 4. Precision Autonomous Landing

**Purpose**: Land drone on target location with accuracy < 10 cm.

**Why Extrinsics Critical**:
- Detect landing marker (ArUco, checkerboard, natural features)
- Estimate marker pose in camera frame
- Transform to body frame using extrinsics
- Control drone based on marker offset in body frame

**Landing Workflow**:
```
Detect landing marker → 
Compute marker pose in camera frame (using PnP) →
Transform to body frame using camera-body extrinsics →
Generate descent trajectory relative to body frame →
Control drone to align with marker
```

**Extrinsics Impact**:
- Position error: Directly affects landing offset
- Rotation error: Causes tilted approach and misalignment
- Typical accuracy requirement: < 2-3 cm position, < 2° rotation

### 5. Obstacle Avoidance

**Purpose**: Real-time collision detection and path planning.

**Why Extrinsics Matter**:
- Detected obstacles must be transformed to world/body frame for planning
- Multiple cameras require consistent transformation
- Time-critical: extrinsic lookup must be fast

**Stereo Obstacle Detection**:
```
Stereo pair → Compute depth for obstacles → 
Transform (u, v, depth) to body frame using extrinsics →
Track obstacle in body frame reference → 
Plan avoidance maneuver
```

### 6. Autonomous Inspection

**Purpose**: Inspect structures (bridges, power lines, buildings) with precision.

**Why Extrinsics Matter**:
- Accurate 3D reconstruction for defect localization
- Geo-referencing: Attach 3D points to GPS locations
- Multiple passes may require consistent extrinsics

**Typical Pipeline**:
```
Fly autonomous path → Capture images at waypoints →
Compute drone GPS + IMU pose at each frame →
Reconstruct 3D points using SfM with extrinsics →
Geo-reference points to world coordinates →
Detect anomalies in reconstructed geometry
```

---

## Calibration Methods

### Method 1: Marker-Based Calibration (ArUco / Chessboard)

#### Principle

Print a known pattern (ArUco marker or checkerboard) and capture images from multiple viewpoints. Solve for extrinsics that best align detected corners/markers with known 3D positions.

#### Advantages
- ✅ Simple, no special equipment needed
- ✅ Fast (few minutes of data collection)
- ✅ Can achieve high accuracy (sub-mm)
- ✅ Well-established in OpenCV

#### Disadvantages
- ❌ Requires printed pattern
- ❌ Must capture pattern from many angles
- ❌ Manual setup required
- ❌ Only gives camera-to-marker extrinsics (need to measure marker-to-body)

#### ArUco Marker Approach (OpenCV)

**Steps**:

1. **Print ArUco board**
```python
import cv2
import cv2.aruco as aruco

board = aruco.getPredefinedDictionary(aruco.DICT_6X6_250)
img = aruco.generateImageMarker(board, markerSize=200)
cv2.imwrite("marker.png", img)
```

2. **Capture images from multiple angles** (≥10 views, various distances/angles)

3. **Detect marker corners in each frame**
```python
detector = aruco.ArucoDetector(board)
corners, ids, rejected = detector.detectMarkers(frame)
```

4. **Estimate camera pose relative to marker** (Perspective-n-Point - PnP)
```python
# For each detected marker
success, rvec, tvec = cv2.solvePnP(
    objectPoints=marker_3d_corners,  # Known 3D positions
    imagePoints=detected_corners_2d,  # Detected image coordinates
    cameraMatrix=K,  # Intrinsic calibration matrix
    distCoeffs=dist_coeffs  # Distortion coefficients
)
# rvec, tvec = extrinsics relative to marker
```

5. **Compute camera-to-body transformation**
```
# Measure or estimate position of marker on calibration rig
# then solve: T_body_to_marker = (measured) 
#            T_body_to_camera = T_body_to_marker × T_marker_to_camera^(-1)
```

#### ChArUco Board (Improved)

ChArUco boards combine chessboard and ArUco markers for better accuracy:

```python
import cv2.aruco as aruco

# Create ChArUco board
board = aruco.getPredefinedDictionary(aruco.DICT_5X5_100)
charuco_board = aruco.CharucoBoard((5, 7), squareLength=0.025, 
                                   markerLength=0.019, dictionary=board)
img = charuco_board.generateImage((200, 280))

# Detect ChArUco corners
detector = aruco.ArucoDetector(board)
corners, ids, rejected = detector.detectMarkers(frame)
ret, charuco_corners, charuco_ids = cv2.aruco.interpolateCornersCharuco(
    corners, ids, frame, charuco_board)

# Calibrate (same as above but with more corners/accuracy)
```

**Advantages of ChArUco**:
- More corners detected per frame (higher accuracy)
- Tolerates partial occlusions
- Automatic corner refinement

#### Practical Implementation Steps

```
1. Print large ArUco/ChArUco board (≥0.3m × 0.3m)
2. Mount camera on drone body with fixed position/orientation
3. Measure approximate camera position and rotation relative to body
4. Capture 20-30 images of board from various distances (0.5-3m) and angles
5. Run calibration script to estimate precise extrinsics
6. Validate on separate image set to check reprojection error
7. Store calibration in YAML/JSON for later use
```

### Method 2: Self-Calibration from Motion

#### Principle

Use image sequence from moving camera without known scene structure. Estimate extrinsics by optimizing consistency of ego-motion estimates between visual and inertial sensors.

#### Key Idea

- **Visual odometry** from camera gives relative pose changes
- **Inertial odometry** from IMU gives relative pose changes
- If extrinsics are correct, both should agree
- Optimize extrinsics to minimize disagreement

#### Advantages
- ✅ No special patterns needed
- ✅ Can run continuously (online refinement)
- ✅ Practical for field deployment
- ✅ Accounts for real-world environmental conditions

#### Disadvantages
- ❌ Requires good visual features + IMU accuracy
- ❌ Slower convergence (hours vs. minutes)
- ❌ Can get stuck in local minima
- ❌ Needs accurate intrinsic calibration beforehand

#### Implementation Strategy

**Offline calibration** (stationary):

```
1. Mount camera on stable platform (tripod/rig)
2. Perform slow, controlled motions for 5-10 minutes
   - Rotate on each axis (roll, pitch, yaw)
   - Translate in each direction
   - Use multiple, distinct trajectories
3. Log camera images and synchronized IMU data
4. Run optimization:
   
   Minimize: Σ ||V_visual(t) - V_imu(t)||^2 + 
             Σ ||Ω_visual(t) - Ω_imu(t)||^2
   
   Over: extrinsic parameters R_imu_cam, t_imu_cam
   
   Where:
   - V = linear velocity (computed from visual odometry / IMU integration)
   - Ω = angular velocity (computed from visual odometry / gyro)
```

**Online refinement** (during flight):

```
1. Start with initial extrinsics (from previous calibration or marker-based)
2. During autonomous operation:
   - Run visual and inertial odometry in parallel
   - Compute error between them (innovation)
   - Slowly update extrinsic estimate if consistent error detected
   - Use adaptive filter (EKF, particle filter)
   
3. Monitor convergence to detect drift/errors
```

**Practical Tools**:
- **Kalibr** (ethz-asl): Industry-standard camera-IMU calibration
  - Uses optimized C++ with graph-based formulation
  - Supports multiple cameras, rolling shutter, asynchronous timestamping
  - GitHub: ethz-asl/kalibr

#### Kalibr Usage Example

```bash
# Record rosbag with camera and IMU
rosbag record -O calib_data.bag /camera/image_raw /imu/data

# Run calibration
kalibr_calibrate_imu_camera \
  --bag calib_data.bag \
  --cam camera_intrinsics.yaml \
  --imu imu_params.yaml \
  --target target.yaml

# Output: camera_imu_calibration.yaml (contains extrinsics)
```

### Method 3: GPS + IMU Ground Truth Approach

#### Principle

Use high-precision global positioning (RTK-GPS) as ground truth to validate camera extrinsics. Compare:
- GPS-integrated position vs. visual position
- Correct extrinsics make them agree

#### Advantages
- ✅ Absolute reference (global coordinates)
- ✅ Works outdoors with good sky visibility
- ✅ Can validate multiple sensors simultaneously
- ✅ High precision with RTK (2-5 cm accuracy)

#### Disadvantages
- ❌ Requires expensive RTK-GPS system
- ❌ GPS denied environments (indoor, urban canyons)
- ❌ Limited to outdoor testing
- ❌ Initialization time (fix convergence)

#### Implementation Strategy

**Hardware Setup**:
```
Drone with:
- Camera (primary sensor to calibrate)
- IMU (standard in all drones)
- RTK-GPS receiver + base station (ground truth)
```

**Calibration Procedure**:

```
1. Fly autonomous pattern maintaining good GPS lock
   - Figure-8 trajectory
   - Circle patterns at varying altitudes
   - Duration: 10-15 minutes
   - Constant velocity (smooth motions help)

2. Log synchronized data:
   - Camera timestamps + images
   - IMU timestamps + acceleration/gyro
   - RTK-GPS timestamps + position/velocity

3. Offline processing:
   
   a) Visual odometry from images → VO trajectory
      - Extract features, match across frames
      - Estimate relative poses using 5-point algorithm
      - Triangulate points
      - Scale using known camera baseline
      
   b) IMU integration → Inertial trajectory
      - Integrate gyro → orientation
      - Integrate accel (with orientation) → velocity
      - Integrate velocity → position
      - Use GPS for bias detection
      
   c) Align trajectories (all to world/GPS frame):
      - VO trajectory is scale-ambiguous (monocular)
      - Use GPS-IMU as reference
      - Apply similarity transform (rotation + scale) to VO
      
   d) Optimize extrinsics:
      
      For each frame i:
      - Compute VO position in body frame: P_VO_body(i)
      - Compute IMU position in body frame: P_IMU_body(i)
      - Extrinsic error: 
        ΔP(i) = R_cam_body × (P_VO_cam - t_cam_body) - P_IMU_body
        
      Minimize: Σ ||ΔP(i)||^2 over R_cam_body, t_cam_body
      
   e) Validate on held-out test trajectory
```

#### Expected Results

With proper RTK-GPS:
- Position error: < 5 cm RMS
- Rotation error: < 2-3 degrees

If extrinsics calibrated correctly, this error should be dominated by:
- GPS noise (5-10 cm)
- Visual feature matching ambiguity
- IMU bias drift

If error is significantly larger, extrinsics likely needs refinement.

#### Multi-Sensor Fusion Approach

Instead of separate optimization, use joint graph-based optimization:

```
Create factor graph with:
- Camera extrinsics as variables
- IMU bias as variables  
- Poses at each timestamp as variables
- Factors from:
  * Visual odometry measurements
  * IMU pre-integration
  * GPS pseudo-range constraints (if available)
  
Solve with g2o or GTSAM
```

This produces maximum likelihood estimate accounting for all uncertainties.

---

## Practical Implementation

### Storage Format for Extrinsics

#### YAML Format (Recommended)

```yaml
# camera_extrinsics.yaml
# Transformation from body frame to camera frame

# Rotation: body → camera
# Represented as rotation matrix (row-major)
rotation_matrix:
  - [0.0, -1.0, 0.0]  # X axis in body frame
  - [0.0,  0.0, 1.0]  # Y axis in body frame
  - [-1.0, 0.0, 0.0]  # Z axis in body frame

# Or as rotation vector (axis-angle, 3 elements)
# Magnitude = rotation angle (radians)
# Direction = rotation axis (normalized)
rotation_vector: [-1.5708, 0.0, 0.0]  # 90° around X axis

# Translation: position of camera optical center in body frame (meters)
translation_vector: [0.0, 0.0, -0.05]  # 5 cm below body origin

# Alternatively, as quaternion [qx, qy, qz, qw]
quaternion: [0.7071, 0.0, 0.0, 0.7071]  # (cos(θ/2) = 0.7071, sin(θ/2)=0.7071)

# Uncertainty/covariance (optional, for filtering)
covariance_rotation: [1e-4, 1e-4, 1e-4]  # radians^2
covariance_translation: [1e-5, 1e-5, 1e-5]  # meters^2

# Timestamp of calibration
calibration_date: "2026-05-07"
calibration_method: "marker-based"
calibration_error_pixels: 0.5
calibration_error_meters: 0.01
```

#### JSON Format

```json
{
  "camera_extrinsics": {
    "rotation_matrix": [
      [0.0, -1.0, 0.0],
      [0.0,  0.0, 1.0],
      [-1.0, 0.0, 0.0]
    ],
    "translation_vector": [0.0, 0.0, -0.05],
    "frame_from": "body",
    "frame_to": "camera",
    "units": "meters",
    "timestamp": "2026-05-07T14:30:00Z",
    "calibration_method": "ArUco_markers",
    "reprojection_error_pixels": 0.5
  }
}
```

### Python Implementation

#### Loading and Using Extrinsics

```python
import numpy as np
import cv2
import yaml

# Load extrinsics from YAML
with open('camera_extrinsics.yaml', 'r') as f:
    calib = yaml.safe_load(f)

R_body_cam = np.array(calib['rotation_matrix'])
t_body_cam = np.array(calib['translation_vector'])

# Inverse: camera to body
R_cam_body = R_body_cam.T
t_cam_body = -R_body_cam.T @ t_body_cam

# Load camera intrinsics
with open('camera_intrinsics.yaml', 'r') as f:
    camera_calib = yaml.safe_load(f)

K = np.array(camera_calib['camera_matrix'])
dist = np.array(camera_calib['distortion_coefficients'])

# Function: pixel → world 3D point
def pixel_to_world(u, v, depth, R_world_body, t_world_body, K, dist):
    """
    Transform pixel coordinate with depth to world frame.
    
    Args:
        u, v: pixel coordinates
        depth: depth from depth sensor (meters)
        R_world_body: rotation from body to world
        t_world_body: translation from body to world
        K: camera intrinsic matrix
        dist: distortion coefficients
    
    Returns:
        P_world: [x, y, z] position in world frame (meters)
    """
    # Undistort pixel
    pixel = np.array([[[u, v]]], dtype=np.float32)
    undistorted = cv2.undistortPoints(pixel, K, dist)
    u_undist, v_undist = undistorted[0, 0]
    
    # Pixel to camera frame
    fx, fy, cx, cy = K[0, 0], K[1, 1], K[0, 2], K[1, 2]
    X_c = (u_undist - cx) * depth / fx
    Y_c = (v_undist - cy) * depth / fy
    Z_c = depth
    P_camera = np.array([X_c, Y_c, Z_c])
    
    # Camera to body frame
    P_body = R_cam_body @ (P_camera - t_cam_body)
    
    # Body to world frame
    P_world = R_world_body.T @ P_body + t_world_body
    
    return P_world

# Function: world 3D point → pixel projection
def world_to_pixel(P_world, R_world_body, t_world_body, K, dist):
    """
    Project world 3D point to pixel image coordinates.
    
    Returns:
        u, v: pixel coordinates
        depth: depth from camera (meters)
    """
    # World to body frame
    P_body = R_world_body @ (P_world - t_world_body)
    
    # Body to camera frame
    P_camera = R_body_cam @ P_body + t_body_cam
    
    # Check if point is in front of camera
    if P_camera[2] <= 0:
        return None, None, None
    
    # Project to pixel
    proj = K @ P_camera
    u = proj[0] / P_camera[2]
    v = proj[1] / P_camera[2]
    depth = P_camera[2]
    
    return u, v, depth

# Function: compute camera pose in world frame
def camera_pose_in_world(R_world_body, t_world_body):
    """
    Compute camera position and orientation in world frame.
    """
    # Camera orientation in world
    R_world_cam = R_world_body @ R_body_cam
    
    # Camera position in world
    P_camera_world = R_world_body @ t_body_cam + t_world_body
    
    return R_world_cam, P_camera_world

# Example usage
if __name__ == "__main__":
    # Drone at [10, 20, 5] meters, level orientation
    t_world_body = np.array([10.0, 20.0, 5.0])
    R_world_body = np.eye(3)
    
    # Get camera pose in world
    R_cam_world, P_cam_world = camera_pose_in_world(R_world_body, t_world_body)
    print(f"Camera position in world: {P_cam_world}")
    print(f"Camera orientation in world:\n{R_cam_world}")
    
    # Project a world point to image
    P_world = np.array([10.0, 20.2, 4.9])  # 20cm away
    u, v, depth = world_to_pixel(P_world, R_world_body, t_world_body, K, dist)
    print(f"World point projects to pixel: ({u:.1f}, {v:.1f}), depth={depth:.2f}m")
```

#### OpenCV Camera-to-World Transformation

```python
import cv2
import numpy as np

def apply_extrinsics_to_points(points_3d, rvec, tvec):
    """
    Apply camera extrinsics (rotation + translation) to 3D points.
    
    Args:
        points_3d: (N, 3) array of 3D points
        rvec: rotation vector (3,) from cv2.solvePnP
        tvec: translation vector (3,) from cv2.solvePnP
    
    Returns:
        transformed: (N, 3) transformed points in reference frame
    """
    # Convert rotation vector to matrix
    R, _ = cv2.Rodrigues(rvec)
    
    # Transform points
    transformed = (R @ points_3d.T + tvec.reshape(3, 1)).T
    return transformed

def get_camera_matrix_from_extrinsics(K, rvec, tvec):
    """
    Create projection matrix from intrinsics and extrinsics.
    P = K [R | t]
    
    This matrix projects 3D world points to 2D image coordinates.
    """
    R, _ = cv2.Rodrigues(rvec)
    Rt = np.hstack([R, tvec.reshape(3, 1)])
    P = K @ Rt
    return P

# Usage
K = np.array([[fx, 0, cx], [0, fy, cy], [0, 0, 1]])
rvec_cam_world = rotation_vector_cam_to_world
tvec_cam_world = translation_vector_cam_to_world

# Compute projection matrix
P = get_camera_matrix_from_extrinsics(K, rvec_cam_world, tvec_cam_world)

# Project 3D points
points_3d = np.array([[X, Y, Z], ...])  # shape: (N, 3)
points_homog = np.hstack([points_3d, np.ones((len(points_3d), 1))])  # (N, 4)
pixels_homog = (P @ points_homog.T).T  # (N, 3)
pixels = pixels_homog[:, :2] / pixels_homog[:, 2:3]  # normalize by Z
```

### Validation Checklist

After calibration, verify:

```
☐ Reprojection error < 2 pixels (good) or < 5 pixels (acceptable)
☐ Camera position in body frame is physically plausible
  - Mounted offset should match mechanical design (±5mm)
  - Should be within visible/accessible region
☐ Rotation matrix satisfies R^T × R = I
  - Check with: np.allclose(R.T @ R, np.eye(3))
☐ Rotation matrix determinant = +1
  - Check with: np.linalg.det(R) ≈ 1.0
☐ Forward-backward projection errors symmetric
  - Project world point to image, then back → should match original
☐ Multiple validation methods agree
  - Marker-based vs. visual odometry vs. GPS
  - Errors should be < 2% of measured distance
☐ Extrinsics stable over time
  - If re-calibrating periodically, changes should be < 5mm, < 1°
```

---

## Summary: Why Extrinsics Matter

| Aspect | Impact of Poor Extrinsics |
|--------|--------------------------|
| **3D Reconstruction** | 3D points incorrectly positioned in world frame; maps are skewed |
| **Autonomous Landing** | Drone misaligns with landing target; landing overshoots/undershoots |
| **Visual Odometry** | Accumulated drift in pose estimates; trajectory diverges from truth |
| **Obstacle Avoidance** | Detected obstacles in wrong positions; collision risk |
| **Inspection** | Defects incorrectly geo-located; follow-up inspections miss targets |
| **Multi-Sensor Fusion** | Visual-inertial estimates disagree; state estimator becomes unreliable |

**Key Takeaway**: Extrinsic calibration is **not optional**—it's the bridge between what the camera sees and where the drone actually is in the world. Accurate extrinsics enable all vision-based autonomous behaviors.

---

## References and Further Reading

### Foundational Papers and Resources

1. **Camera Models and Extrinsic Parameters**
   - Dissecting the Camera Matrix: https://ksimek.github.io/2012/08/22/extrinsic/
   - Multiple View Geometry Textbook: Hartley & Zisserman

2. **Calibration Techniques**
   - OpenCV Camera Calibration: https://docs.opencv.org/4.x/da/d0d/tutorial_camera_calibration_pattern.html
   - ArUco Calibration: https://docs.opencv.org/3.4/da/d13/tutorial_aruco_calibration.html
   - ChArUco Boards: https://medium.com/@nflorent7/a-comprehensive-guide-to-camera-calibration-using-charuco-boards-and-opencv-for-perspective-9a0fa71ada5f

3. **Visual Odometry and SLAM**
   - VINS-Mono (monocular visual-inertial SLAM): https://github.com/HKUST-Aerial-Robotics/VINS-Mono
   - Visual-Inertial Odometry: https://www.thinkautonomous.ai/blog/visual-inertial-odometry/
   - Structure from Motion: https://cmsc426.github.io/gtsam/

4. **Drone Applications**
   - Autonomous Landing with Vision: https://www.mdpi.com/2226-4310/9/11/634
   - Visual Odometry for UAVs: https://www.uavnavigation.com/company/blog/visual-odometry
   - Monocular Visual-Inertial System: https://medium.com/@rustamubajdulloev4/visual-inertial-odometry-system-for-a-drone-in-c-c84fee5f8564

5. **3D Reconstruction**
   - Epipolar Geometry: https://en.wikipedia.org/wiki/Epipolar_geometry
   - 3D Reconstruction from Epipolar Geometry: https://www.cs.auckland.ac.nz/courses/compsci773s1c/lectures/CS773S1C-3DReconstruction.pdf

6. **Multi-Sensor Calibration**
   - Kalibr (Camera-IMU Calibration Tool): https://github.com/ethz-asl/kalibr
   - Camera-IMU Extrinsic Calibration: https://github.com/ethz-asl/kalibr/wiki/camera-imu-calibration
   - Camera, LiDAR, and IMU Calibration: https://www.mdpi.com/1424-8220/25/17/5409
   - MATLAB Extrinsic Calibration: https://www.mathworks.com/help/nav/ug/estimate-camera-to-imu-transformation-using-extrinsic-calibration.html

7. **Robotics Coordinate Frames**
   - Robot Coordinate Systems: https://techietory.com/robotics/understanding-coordinates-and-reference-frames-in-robotics/
   - Geometry and Reference Frames: https://dev.bostondynamics.com/docs/concepts/geometry_and_frames.html
   - ROS Transforms: https://foxglove.dev/blog/understanding-ros-transforms

8. **Computer Vision Theory**
   - Camera Resectioning: https://en.wikipedia.org/wiki/Camera_resectioning
   - Extrinsic Parameters (ScienceDirect): https://www.sciencedirect.com/topics/engineering/extrinsic-parameter

9. **Vision AI Calibration**
   - Vision AI Camera Calibration Guide: https://blog.roboflow.com/vision-ai-camera-calibration/
   - Camera-Robot Extrinsic Calibration: https://samarth-robo.github.io/blog/2020/11/18/robot_camera_calibration.html

10. **Real-World UAV Calibration**
    - Camera Calibration Accuracy at Different UAV Heights: https://www.researchgate.net/publication/313939799_CAMERA_CALIBRATION_ACCURACY_AT_DIFFERENT_UAV_FLYING_HEIGHTS
    - Two-Step Camera Calibration for Micro UAVs: https://www.researchgate.net/publication/307531025_Two-step_camera_calibration_method_developed_for_micro_UAV's

---

**Document Version**: 1.0  
**Last Updated**: 2026-05-07  
**Author**: Research and Documentation  
**Status**: Complete Reference Guide
