# MobileDevice.framework can live in two locations, the first is blocked from being accessed when
# building so we need to copy it to our build directory to ensure that we can use it during the
# build process. The second location is seemingly for updated versions of the framework and the
# first location will actually become a symlink to this destination if it exists.

MOBILEDEVICE_LOCATION=$1
MOBILEDEVICE_NEW_LOCATION=$2
BUILD_DIRECTORY=$3
MOBILEDEVICE_TO_COPY=""

if [ -d $MOBILEDEVICE_LOCATION ]; then
    MOBILEDEVICE_TO_COPY=$MOBILEDEVICE_LOCATION
fi

if [ -d $MOBILEDEVICE_NEW_LOCATION ]; then
    MOBILEDEVICE_TO_COPY=$MOBILEDEVICE_NEW_LOCATION
fi

if [ -z "$MOBILEDEVICE_TO_COPY" ]; then
    echo "Could not find MobileDevice.framework"
    exit 1
else
    cp -RL $MOBILEDEVICE_TO_COPY $BUILD_DIRECTORY
fi
