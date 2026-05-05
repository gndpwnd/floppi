ok so this repo is for more drone research and i amd trying to setup a few different projects...

/home/devel/floppi/flight_controller

flight controller is for a universal VTOL flight controller that is barebones and you can read its scope file if you want...

/home/devel/floppi/drone_3d_model

the 3d model stuff is for researching best VTOL frames and models for different applications like long range vs maneuverability and other things 

/home/devel/floppi/fc_tool

fc tool is supposed to be an alternative serial monitoring tool that is simply more versatile and easier to work with, not a priority to continue right now and i will say why here soon

/home/devel/floppi/swarm_api

swarm api is supposed to be an idea where we bridge the gap from drone swarm hardware into a single central control point to be able to interact with a physical "swarm" as part of mission control c2 and stuff... this is also not a priority for now since there is no swarm yet...


/home/devel/floppi/auto_orientation

auto orientation is the priority project for now... i have added a markdown file with context inside the folder... 


The main idea is to well actually I have a lot of projects that use an IMU or magnetometers or accelerometers and gyroscopes and stuff and basically there's a whole different list of applications but the idea is that we need to make like a toolkit or like a list of scripts or figure out the math so that we can basically take an accelerometer or an IMU with a magnetometer whatever sort of sensor combination and to stick it onto a device and figure out absolute orientation relative to the Earth if we have a magnetometer or just in general if we don't have a magnetometer just figure out what axes point where on the sensor does that make sense? so the idea for an application on something like a drone would be to see like calibrating a flight controller automatically and then the application could be on cameras so that way we could have like a sensor kit that we put on a camera rotated a few times or whatever and figure out where the camera is if we hook up a GPS and we'll know the latitude longitude and potentially altitude but then also the orientation of the camera in space which is extremely critical for machine learning applications And especially for being able to reconstruct positions from image layers into 3D space given skew lines and stuff like that there's another project located at

~/skytracker-algorithm
~/skytracker_data_analysis

Feel free to explore these a little bit and understand how the camera setup works if you want to and then also the flight controller that I'm trying to set up is located in this repository at 

/home/devel/floppi/flight_controller

And maybe even from the flight controller code and documentation you'll get an idea or an inspiration for how to figure out the auto calibration is just that this feature of Auto calibration or Auto orientation calibration is such a fundamental thing that we really need to get it right and it's really hard to implement it into projects that already exist like the flight controller or the camera projects the sky tracker stuff it's just we need to figure out the core fundamental Auto orientation stuff and that's why I'm setting up a dedicated project for it and basically I want to be able to take an mpu 6050 and I'm unsure of the model number of the specific Arduino magnetometer sensor that I'm using but I'll be able to throw that in there as well and then another set up would be like the B&O the stuff from the context of the email given to you you'll be able to see what sensors and what combinations of stuff I will be using but we want to make it so that we can use it across multiple different types of sensors whatever combination really of a gyroscope accelerometer and a magnetometer and basically getting like a x and a y and a z and all the other variables and then also possibly absolute orientation or being able to calculate absolute orientation if you can't do that with certain sensors or like they're firmware doesn't support it then please research how we can do those calculations for different bits of information we might be missing or derivations we might be missing along the way so there's a whole lot of math and I need to set up a lot of documentation and please do a bunch of web research and all this stuff and basically I'm just trying to set up this entire Auto orientation project to be able to handle all that and especially try to get Dr Comper’s (MDC)  sensors working with the persistent storage of calibration values and stuff like that I also think I have an SD card module that I can use of course with the bno sensor we're going to try to use the persistent memory but then for like the mpu 6050 we're going to try and use an SD card if it doesn't have a persistent memory again I don't know you might want to do web research and spawn agents and document on your findings but either way this is kind of context for setting up this project and how I want to structure it and you're going to have a lot of questions so please ask those questions you might not have enough context but I'm trying to get this project underway so that way we can have the foundation of the auto orientation and helping to automate calibration processes so that we can be using this type of sensor clustering across so many applications there's a lot of things so many projects with so many applications of this that I want to get underway it's not even funny so yes here's a guy to set up the project initially and we'll go from there how does that sound?

To clarify we are sticking in the auto orientation folder and setting up this own integrated project into the Repository


And then Also to clarify I included in Arduino sketch for the bno85 as well we already kind of got something working in terms of like a sensor cluster and to specify I want to like archive the Arduino sketch and then also doctor compier's initial email into the archive folder that would get set up and then reference them in like the scope file and the roadmap file that'll get set up or whatever basically just relevant documentation so that way we can or what I'm looking to do is have you set up a comprehensive platformio project so that way we can make as much more modular and much easier to handle across different sensors and different libraries and ideally I would go out and find the specific libraries on GitHub and download them to my machine locally so that we were not having to depend on the cloud all the time and we can build the project completely locally so special when we're out in the field of making code changes we can do all that but otherwise yes please get a start and especially initialize the platformio project and stuff and make sure to update the git ignore file I mean if you have any questions but yes I definitely listed all the sensors that I know we're going to try to be using and let me know when you're ready to run the sketch on the current sensor cluster we have on the bno 85 also note that the GPS module I have is over USB I don't have it plugged in yet but let me know when you're ready to plug that in and we could like make Python scripts to try to figure out what's going on with the GPS USB module and also another thing is please use Python scripts to monitor serial connections with platformio again some scripts might already exist and you could either copy them or use the inspiration from the flight controller project also in this repository maybe you look through that and see what scripts they use for serial monitoring and stuff because if you go out and do manual commands for serial monitoring you will get stuck let's see if I can find the serial monitoring script right quick or stuff that is used like it maybe you can make a copy or something. 

Yes take a look through the following folder so that you can get like the serial monitor python script and whatever and be able to understand how to use those because we need to get this Auto orientation project underway and starting developing things so that way developers can continue on the flight controller project as needed but again focus on the auto orientation project.
/home/devel/floppi/flight_controller/tools


Here is a guide to get it setup in the standardized way i prefer:

"""
does this make sense? Feel free to ask clarifying questions as needed. Here is a guide.

Initialize this project.

=== WHAT THE LLM/AGENT WILL DO ===

Reference guides (read before proceeding):
- ~/llm-project-bootstrap/guides/SESSION_CONDUCT.md (operational conventions)
- ~/llm-project-bootstrap/guides/PROJECT_SETUP.md (project structure)
- ~/llm-project-bootstrap/guides/DOCUMENTATION_HANDLING.md (file organization)
- ~/llm-project-bootstrap/guides/AUTONOMOUS_OPERATION.md (goal-horizon structure)
- ~/llm-project-bootstrap/guides/PROJECT_SCRIPTS.md (install, test, deploy script planning)
- ~/llm-project-bootstrap/templates/ - use for document formats
- ~/llm-project-bootstrap/context/ - check available resources

Create project structure:
- docs/scope.md - project boundaries (ASK before finalizing)
- docs/roadmap.md - feature plan with milestones (ASK before finalizing)
- docs/todo.md - current tasks
- docs/README.md - project overview
- docs/features/ - feature specifications (what it does, APIs, usage)
- docs/findings/ - research and discoveries (what we learned)
- docs/archive/ - session summaries

Goal-horizon setup (embed in roadmap):
- Define first stable release milestone (midterm goal)
- Identify what "deployable/testable" means for this project
- Separate must-have features from nice-to-have
- The roadmap should support future LLMs in selecting discovery vs development vs stability mode

Script infrastructure planning (embed in roadmap per ~/llm-project-bootstrap/guides/PROJECT_SCRIPTS.md):
- Will users need to install dependencies? → Plan installation script
- Will developers need to test changes? → Plan pytest-based modular testing (pytest works for any project type)
- Will this be deployed somewhere? → Plan deployment script with diagnostics
- Add brief roadmap entries with platform and language context

Process:
1. If context provided → draft scope and roadmap, ask for confirmation
2. If no context → ask questions first to understand the project
3. Research as needed to inform roadmap
4. Document findings in docs/findings/

The LLM/agent will ASK before making scope decisions - human operator defines what's in/out.


""
