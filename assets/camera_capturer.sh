#!/bin/sh

# This script is responsible for capturing video from camera using raspivid (because it allows for
# much finer tuning of camera parameters) and encapsulation in MPEG-TS container via FFmpeg that
# will segment files in HLS-compatible way. Produced segment files are ready for uploading to
# remote server where they can be stored or feed live to clients.

# export $(grep -v '^#' /etc/default/camera | xargs)

[ "$CAMERA_BASE_URL" != "" ] || exit 1
[ "$CAMERA_BASE_DIR" != "" ] || exit 1

[ "$CAMERA_WIDTH" -eq "$CAMERA_WIDTH" ] 2>/dev/null && [ $CAMERA_WIDTH -gt 0 ] && [ $CAMERA_WIDTH -le 1920 ] || exit 1
[ "$CAMERA_HEIGHT" -eq "$CAMERA_HEIGHT" ] 2>/dev/null && [ $CAMERA_HEIGHT -gt 0 ] && [ $CAMERA_HEIGHT -le 1080 ] || exit 1

[ "$CAMERA_FPS" -eq "$CAMERA_FPS" ] 2>/dev/null && [ $CAMERA_FPS -gt 0 ] && [ $CAMERA_FPS -le 30 ] || exit 1
[ "$CAMERA_SEGMENT_DURATION" -eq "$CAMERA_SEGMENT_DURATION" ] 2>/dev/null && [ $CAMERA_SEGMENT_DURATION -gt 0 ] || exit 1

trap 'kill -TERM $PID' INT TERM

raspivid \
    -fps $CAMERA_FPS -w $CAMERA_WIDTH -h $CAMERA_HEIGHT -n -t 0 -pf baseline -lev 4 \
    -qp 20 -b 0 -g $(($CAMERA_FPS*$CAMERA_SEGMENT_DURATION)) -cfx 128:128 -a 1032 -a "%Y-%m-%d %X" -o - | \
    ffmpeg -loglevel fatal \
        -r $CAMERA_FPS -f h264 -probesize 32 -analyzeduration 1 -i - -codec:v copy -f hls \
        -hls_list_size 0 -hls_time $(($CAMERA_SEGMENT_DURATION-1)) -use_localtime 1 -use_localtime_mkdir 1 \
        -hls_flags second_level_segment_index+temp_file+independent_segments \
        -hls_segment_filename "$CAMERA_BASE_DIR/%Y-%m-%d/video-%H:%M:%S-%%04d.ts" \
        /dev/null

# This trickery is needed for properly killing child processes (ffmpeg & raspivid) so that they
# can be restarted quickly. In this instance, we'll kill ffmpeg process, which will trigger end
# of raspivid process.
# Idea from http://veithen.io/2014/11/16/sigterm-propagation.html

PID=$!
wait $PID
trap - INT TERM
wait $PID

exit 0;
