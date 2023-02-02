# After testing the setup in my office for a while I moved it to the bedroom where a cat lives nearly full time on the end of the bed in clear view of both of my mmWave sensors:

## DFRobot - mmWave - Human Presence Detection Sensor - SEN0395
vs.
## Seeed Studio - 60GHz mmWave Sensor - Fall Detection Pro Module - MR60FDA1

![Non-human Movement](/static/images/non-human%20movement.png)

The cat is quite small and old and she doesn't move much but you can clearly see that even after just one hour of testing, the DRFobot sensor is picking up on much more movement but the Seeed sensor is only triggering once in the entire time. I looked at the video from that time frame and it's when the cat gets up and walks around on the bed. Before that she was barely moving around and laying still, yet the DFRobot is flapping motion detection.

The interesting thing for me, and could mean some more refined automation control... is that even when the Seeed sensor was showing Presence, it was not showing motion the entire time which, after checking the status messages, it revealed that it was in fact showing "Stationary" motion state which I send via MQTT as motion=clear but that's beside the point. What this means is you could use presence+motion to trigger lights, etc... conditionally. And then use presence, which is far less flappy with this sensor, to determine occupancy. This could eliminate the need for combo mmWave + PIR setups.
