ADD_EXECUTABLE(take_picture take_picture.c)

ADD_EXECUTABLE(yuyv_show yuyv_show.c)

ADD_EXECUTABLE(mjpg_show mjpg_show.c)
TARGET_LINK_LIBRARIES(mjpg_show -ljpeg)
