import os
import sys
import time
import pysher
import json
import urllib.request
import subprocess
import serial

import logging
root = logging.getLogger()
root.setLevel(logging.INFO)
ch = logging.StreamHandler(sys.stdout)
root.addHandler(ch)

from dotenv import load_dotenv
load_dotenv()

pusher = pysher.Pusher( os.getenv("PUSHER_KEY"), 'us2' )

uploadCmd = "esptool -vv -cd nodemcu -cb 921600 -cp /dev/ttyUSB0  -ca 0x00000 -cf LED_Sectional.ino.bin"

def  deploy_complete( *args, **kwargs ):
    jsonObj = json.loads( args[0] )
    package = jsonObj['package']['name']
    version = jsonObj['version']['name']
    file = "LED_Sectional.ino.bin"
    url = "https://dl.bintray.com/project802/led-sectional/" + package + "/" + version + "/" + file

    if os.path.exists( file ):
        os.remove( file )

    print( "Pausing for bintray to propogate access rights" )
    time.sleep( 10 )
    print( "Downloading " + url )
    urllib.request.urlretrieve( url, file )
    print( "Done" )
    subprocess.run( uploadCmd.split() )

    ser = serial.Serial( '/dev/ttyUSB0', 115200, timeout=30 )
    while True:
        out = ser.read_until().decode( "utf-8" )
        print( out )
        if len(out) == 0:
          print( "Error - did not get to time setting" )
          break
        if out.find( "Time is now " ) != -1:
          print( "Success!" )
          break;

    ser.close()
    print( "Done with CI" )

# We can't subscribe until we've connected, so we use a callback handler
# to subscribe when able
def connect_handler(data):
    channel = pusher.subscribe( 'travis' )
    channel.bind( 'deploy_complete', deploy_complete )

pusher.connection.bind( 'pusher:connection_established', connect_handler )
pusher.connect()

while True:
    # Do other things in the meantime here...
    time.sleep( 1 )
