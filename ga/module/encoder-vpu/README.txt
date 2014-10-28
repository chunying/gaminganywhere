
-- you will need imx-lib-ubt0 package on UDOO's Ubuntu Linux


1. Call vpu_Init() to initialize the VPU.
2. Open a encoder instance by using vpu_EncOpen().
3. Before starting a picture encoder operation, get crucial parameters for encoder operations such as required frame buffer
size by using vpu_EncGetInitialInfo().
4. By using the returned frame buffer requirement, allocate size of frame buffers and convey this information to the VPU
by using vpu_EncRegisterFrameBuffer().
5. Generate high-level header syntax by using vpu_EncGiveCommand().
6. Start picture encoder operation picture-by-picture by using vpu_EncStartOneFrame().
7. Wait the completion of picture encoder operation interrupt event.
8. After encoding a frame is complete, check the results of encoder operation by using vpu_EncGetOutputInfo().
9. If there are more frames to encode, go to Step 4. Otherwise, go to the next step.
10. Terminate the sequence operation by closing the instance using vpu_EncClose().
11. Call vpu_UnInit() to release the system resources.

