#! /bin/sh

if [ $BUILDDIR -z ]; then
    BUILDDIR=../vlc-outoftree/build/win64/vlc-4.0.0-dev
fi

"$BUILDDIR"/vlc.exe --play-and-exit --intf none --dec-dev none --avcodec-hw none --no-spu --no-osd --vout vk --video-filter=ravu-fp --ravu-render --scale 2 --file-logging --logfile=ravu-log.txt $@
echo "RAVU CPU FP-32:        `cat ravu-log.txt | grep micro-seconds`"
rm ravu-log.txt

"$BUILDDIR"/vlc.exe --play-and-exit --intf none --dec-dev none --avcodec-hw none --no-spu --no-osd --vout vk --video-filter=ravu --ravu-render --scale 2 --no-vnni --file-logging --logfile=ravu-log.txt $@
echo "RAVU CPU WITHOUT VNNI: `cat ravu-log.txt | grep micro-seconds`"
rm ravu-log.txt

"$BUILDDIR"/vlc.exe --play-and-exit --intf none --dec-dev none --avcodec-hw none --no-spu --no-osd --vout vk --video-filter=ravu --ravu-render --scale 2 --file-logging --logfile=ravu-log.txt $@
echo "RAVU CPU WITH VNNI:    `cat ravu-log.txt | grep micro-seconds`"
rm ravu-log.txt

"$BUILDDIR"/vlc.exe --play-and-exit --intf none --dec-dev none --avcodec-hw none --no-spu --no-osd --vout vk --vk-ravu --file-logging --logfile=ravu-log.txt $@
echo "RAVU GPU:              `cat ravu-log.txt | grep micro-seconds`"
rm ravu-log.txt
