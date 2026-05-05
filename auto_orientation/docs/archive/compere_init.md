GPS + IMU summer challenge

Hello Gatlin,

 

Thanks for picking up these two sensors. I’d like to start the software and sensing development now for a deployable pair of cameras that auto-calibrate their location and orientation in the earth’s NED frame (North East Down).

 

The first is a Ublox NEO-M9N with datasheet here. Bizzarro fact is that I got it from a place online from Croatia. I‘m pretty sure it’s this device. They’ve got a ton of amazing dev boards with really clean interfaces like the usb.

 

The second is a low cost IMU called the BNO085 from Adafruit with a product page here and the hookup guide documentation here.


The BNO085 needs to be calibrated almost each time it is powered on. I’ve wanted for a few years to figure out how to save the magnetometer calibration settings so it ‘just knows’ where it is when it wakes up from powered off. I’ve never figured this out and the earlier predecessor the BNO055 had a less mature api.

 

Your OpenClaw can probably figure both of these out really well.

 

On the GPS, you’re right, standard GPS only has a nominal accuracy of about +/-1m, but if you know the GPS is stationary, you can take samples and improve the position estimate. I’ve never done this but I know it’s possible. So over time, say 10 minutes or 30 minutes you can improve accuracy, somehow. There is a metric called CEP, which stands for the 95th percentile Circular Error Probability. There are ways to take muiltiple samples, when the GPS nit is stationary and reduce the error down to, say, 0.1m. I’ve never done this but I’m hoping you and OpenClaw can figure this out. The UWB are an alterative but GPS is super easy. GPS is pretty good so lets start with those.

 

On the BNO085, the ideal result would be reading absolute orientation in quaternions. It will output quaternions directly, which is 4 floats. The first question anyone asks about quaternions is which is the scalar element: the first or the last? The ‘fusion’ mode is best, and I don’t thnk the Robot Vacuum Cleaner (RVC) mode is ideal. That RVC mode is for robot vacuum cleaners that have very little pitch or roll. The deployable devices will need to handle significant roll and pitch, so the more general fusion mode is better.


Also, the UART mode for the non-RVC mode is what I’ve wired up. I think the best output vector is the “Absolute Orientation” from the possible list of rotation vectors (not Geomagnetic and not Game Rotation vectors).

 

The World Magnetic Model (WMM) magnetic calculator we used was this. Yaw is significantly more error prone than pitch and roll because of two features. First, there is no acceleration in the “yaw” direction. Gravity is only down so fusion between the magnetometer and accelerometer allows the accel to ‘help’ with pitch and roll but not yaw. Second, to determine which way is north, the magnetometer is (on it’s own because accel cannot help) and because the Largest component is the “Up” component of the earth’s magnetic field. The East-West and North-South components are about 1/3 the size of the “Up” component. So the earth’s magnetic field (at least in Florida) is harder to detect in yaw.


I’ve placed the BNO085 in UART mode (not UART-RVC mode) by bringing the P1 pin high (5V). Then if you wire it up like this for UART wiring it will report values over a standard serial connection like the code that I’ve attached.


The addjino sketch we tested this evening is attached. It relies on the Adafruit BNO08x library (not the RVC) and you can get that from the Ide | Manage Libraries | search for BNO08. When you go to Alaska or anywhere different with the BNO after calibrating it, you’ll need to revalibrate. Any distance far enough to change the magnetic declination angle will need recalibration.


Let me know when you get some results,

 

MDC
