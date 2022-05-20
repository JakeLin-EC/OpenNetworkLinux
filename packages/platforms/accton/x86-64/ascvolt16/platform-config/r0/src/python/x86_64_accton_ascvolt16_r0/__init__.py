from onl.platform.base import *
from onl.platform.accton import *
from time import sleep

class OnlPlatform_x86_64_accton_ascvolt16_r0(OnlPlatformAccton,
                                              OnlPlatformPortConfig_20x100):
    PLATFORM='x86-64-accton-ascvolt16-r0'
    MODEL="ASCVOLT16"
    SYS_OBJECT_ID=".volt.16"

    def baseconfig(self):
        self.insmod('optoe')
        for m in [ 'sys' , 'cpld', 'fan', 'psu', 'leds', 'thermal' ]:
            self.insmod("x86-64-accton-ascvolt16-%s.ko" % m)

        ########### initialize I2C bus 0 ###########
        self.new_i2c_devices([
                # initialize multiplexer (PCA9548)
                ('pca9548', 0x74, 0), # i2c 1-8
                
                # initialize multiplexer (PCA9548) of COMe Connector
                ('pca9548', 0x70, 1), # i2c 9-16
                ('pca9548', 0x73, 2), # i2c 17-24

                # initiate  multiplexer (PCA9548) for Transceiver ports
                ('pca9548', 0x72, 9),  # i2c 25-32
                ('pca9548', 0x72, 10), # i2c 33-40
                ('pca9548', 0x76, 11), # i2c 41-48
                ('pca9548', 0x77, 12), # i2c 49-56

                #initiate CPLD
                ('ascvolt16_cpld', 0x60, 56),
                ])

        # initialize pca9548 idle_state in kernel 5.4.40 version
        subprocess.call('echo -2 | tee /sys/bus/i2c/drivers/pca954x/*-00*/idle_state > /dev/null', shell=True)
        # initialize QSFP28 port(0-1), SFP28 port(2-9), GPON port(10-25)
        port_i2c_bus = [49, 50, 41, 42, 43, 44, 45, 46, 47, 48,
                        25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
                        35, 36, 37, 38, 39, 40]

        # initialize Transceiver port 0-25
        for port in range(0, 26):
            self.new_i2c_device('optoe2', 0x50, port_i2c_bus[port])
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port_i2c_bus[port]), shell=True)

        # initialize Transceiver port 10-25
        for port in range(10, 26):
            self.new_i2c_device('optoe2', 0x58, port_i2c_bus[port])
            subprocess.call('echo port%d > /sys/bus/i2c/devices/%d-0050/port_name' % (port, port_i2c_bus[port]), shell=True)

                
        return True
