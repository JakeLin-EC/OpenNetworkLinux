/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2017 Accton Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/sfpi.h>
#include <onlplib/i2c.h>
#include <onlplib/file.h>
#include "platform_lib.h"

#define EEPROM_I2C_ADDR     0x50
#define EEPROM_START_OFFSET 0x0
#define NUM_OF_SFP_PORT     26

#define VALIDATE_SFP(_port) \
    do { \
        if (_port < 2 || _port > 9) \
            return ONLP_STATUS_E_UNSUPPORTED; \
    } while(0)

#define VALIDATE_SFP_GPON(_port) \
    do { \
        if (_port < 2 || _port > 25) \
            return ONLP_STATUS_E_UNSUPPORTED; \
    } while(0)

#define VALIDATE_QSFP(_port) \
    do { \
        if (_port < 0 || _port > 1) \
            return ONLP_STATUS_E_UNSUPPORTED; \
    } while(0)

static const int port_bus_index[NUM_OF_SFP_PORT] = {
    49, 50, 41, 42, 43, 44, 45, 46, 47, 48,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
    35, 36, 37, 38, 39, 40
};

#define PORT_BUS_INDEX(port) (port_bus_index[port])
#define PORT_FORMAT "/sys/bus/i2c/devices/%d-0050/%s"
#define PORT_EEPROM_FORMAT "/sys/bus/i2c/devices/%d-0050/eeprom"
#define PORT_COMBO_EEPROM_FORMAT "/sys/bus/i2c/devices/%d-0058/eeprom"
#define MODULE_RESET_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_reset_%d"
#define MODULE_LPMODE_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_lpmode_%d"
#define MODULE_PRESENT_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_present_%d"
#define MODULE_RXLOS_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_rx_los_%d"
#define MODULE_TXDISABLE_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_tx_disable_%d"
#define MODULE_TXFAULT_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_tx_fault_%d"
#define MODULE_GPONTYPE_MAIN_BOARD_CPLD_FORMAT "/sys/bus/i2c/devices/56-0060/module_gpon_type_%d"

int
is_combo(int port){
    FILE* fp;
    char file[64] = {0};
    char data[256];
    int ret;
    int combo = 0;
     /* Check gpon type */
    sprintf(file, MODULE_GPONTYPE_MAIN_BOARD_CPLD_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if (fp) {
        ret = fread(data, 1, 1, fp);
        /* gpon: 7, combo: 4, xgs: 3 */
        if (data[0] == '4') {
            combo = 1;
            fclose(fp);
            return combo;
        }
    }
    fclose(fp);

    /* Check eeprom(0x58) */
    sprintf(file, PORT_COMBO_EEPROM_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if (fp) {
        ret = fread(data, 1, 256, fp);
        if(ret){
            combo = 1;
            fclose(fp);
            return combo;
        }
    }
    fclose(fp);

    /* Check Part Number */       
    sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if (fp) {
        /* Read Vendor Name */
        fseek(fp, 20, SEEK_SET);
        ret = fread(data, 1, 16, fp);
        if (strncmp(data, "Hisense", strlen("Hisense")) == 0){
            /* Read Part Number */
            fseek(fp, 40, SEEK_SET);
            ret = fread(data, 1, 16, fp);
            if (strncmp(data, "LTF5308B", strlen("LTF5308B")) == 0){
                combo = 1;
                fclose(fp);
                return combo;
            }
        }
    }
    fclose(fp);
    return combo;
}

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/
int
onlp_sfpi_init(void)
{
    /* Called at initialization time */
    return ONLP_STATUS_OK;
}

int
onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
    /*
     * Ports {0, 73}
     */
    int p;
    AIM_BITMAP_CLR_ALL(bmap);

    for(p = 0; p < NUM_OF_SFP_PORT; p++) {
        AIM_BITMAP_SET(bmap, p);
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_is_present(int port)
{
    /*
     * Return 1 if present.
     * Return 0 if not present.
     * Return < 0 if error.
     */
    int present, ret;
    FILE* fp;
    char file[64] = {0};
    uint8_t data[256];
    char *path = NULL;

    switch (port) {
    case 0 ... 9:
        path = MODULE_PRESENT_MAIN_BOARD_CPLD_FORMAT;
        if (onlp_file_read_int(&present, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n", port);
            return ONLP_STATUS_E_INTERNAL;
        }
        break;
    case 10 ... 25:
        sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
        fp = fopen(file, "r");
        if(fp == NULL) {
            present = 0;
            return present;
        }

        ret = fread(data, 1, 256, fp);
        if (ret != 256) {
            present = 0;
            fclose(fp);
            return present;
        }
        present = 1;
        fclose(fp);
        break;
    default:
        return ONLP_STATUS_E_INVALID;
    }

    return present;
}

int
onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    int i=0, val=0;
    char *path = NULL;
    /* Populate bitmap */
    for (i = 0; i < NUM_OF_SFP_PORT; i++) {
        val = 0;
        switch (i) {
        case 0 ... 25:
            path = MODULE_RXLOS_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if ((i >= 2) && (i <= 9)) {
            if (onlp_file_read_int(&val, path, i+1) < 0) {
                AIM_LOG_ERROR("Unable to read rx_loss status from port(%d)\r\n", i);
            }

            if (val)
                AIM_BITMAP_MOD(dst, i, 1);
            else
                AIM_BITMAP_MOD(dst, i, 0);
        }
        else {
            AIM_BITMAP_MOD(dst, i, 0);
        }
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
    /*
     * Read the SFP eeprom into data[]
     *
     * Return MISSING if SFP is missing.
     * Return OK if eeprom is read
     */
    int size = 0;
    if (port < 0 || port >= NUM_OF_SFP_PORT) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (onlp_file_read(data, 256, &size, PORT_FORMAT, PORT_BUS_INDEX(port), "eeprom") != ONLP_STATUS_OK) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (size != 256) {
        AIM_LOG_ERROR("Invalid file size(%d)\r\n", size);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_dom_read(int port, uint8_t data[256])
{
    FILE* fp;
    char file[64] = {0};

    sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if(fp == NULL) {
        AIM_LOG_ERROR("Unable to open the eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (fseek(fp, 256, SEEK_CUR) != 0) {
        fclose(fp);
        AIM_LOG_ERROR("Unable to set the file position indicator of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    int ret = fread(data, 1, 256, fp);
    fclose(fp);
    if (ret != 256) {
        AIM_LOG_ERROR("Unable to read the module_eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_readb(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, uint8_t value)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_writeb(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_readw(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, uint16_t value)
{
    int bus = PORT_BUS_INDEX(port);
    return onlp_i2c_writew(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
    int rv = ONLP_STATUS_OK;
    char *path = NULL;
    switch(control) {
    case ONLP_SFP_CONTROL_TX_DISABLE:
    {
        VALIDATE_SFP_GPON(port);

        switch (port) {
        case 0 ... 9:
            path = MODULE_TXDISABLE_MAIN_BOARD_CPLD_FORMAT;
            break;
        case 10 ... 25:
            path = MODULE_TXDISABLE_MAIN_BOARD_CPLD_FORMAT;
            if (is_combo(port)) 
                return rv;
            break;
        default:
            break;
        }
        if (onlp_file_write_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to set tx_disable status to port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    }
    case ONLP_SFP_CONTROL_RESET: {
        VALIDATE_QSFP(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_RESET_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_write_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to write reset status to port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    }
    case ONLP_SFP_CONTROL_LP_MODE: {
        VALIDATE_QSFP(port);
        switch (port) {
        case 0 ... 25:
            path = MODULE_LPMODE_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_write_int(value, MODULE_LPMODE_MAIN_BOARD_CPLD_FORMAT, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to write lpmode status to port(%d)\r\n", port);
            return ONLP_STATUS_E_INTERNAL;
        }
    }
    default:
        rv = ONLP_STATUS_E_UNSUPPORTED;
        break;
    }

    return rv;
}

int
onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
    int rv = ONLP_STATUS_OK;
    char *path = NULL;
    switch(control) {
    case ONLP_SFP_CONTROL_RX_LOS:
    {
        VALIDATE_SFP(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_RXLOS_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_read_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read rx_loss status from port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    }
    case ONLP_SFP_CONTROL_TX_FAULT:
        VALIDATE_SFP_GPON(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_TXFAULT_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_read_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read tx_fault status from port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    case ONLP_SFP_CONTROL_TX_DISABLE:
    {
        VALIDATE_SFP_GPON(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_TXDISABLE_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_read_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read tx_disabled status from port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    }
    case ONLP_SFP_CONTROL_RESET:
    {
        VALIDATE_QSFP(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_RESET_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_read_int(value, path, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read reset status from port(%d)\r\n", port);
            rv = ONLP_STATUS_E_INTERNAL;
        }
        break;
    }
    case ONLP_SFP_CONTROL_LP_MODE:
    {
        VALIDATE_QSFP(port);

        switch (port) {
        case 0 ... 25:
            path = MODULE_LPMODE_MAIN_BOARD_CPLD_FORMAT;
            break;
        default:
            break;
        }
        if (onlp_file_read_int(value, MODULE_LPMODE_MAIN_BOARD_CPLD_FORMAT, (port+1)) < 0) {
            AIM_LOG_ERROR("Unable to read lomode status from port(%d)\r\n", port);
            return ONLP_STATUS_E_INTERNAL;
        }
    }
    default:
        rv = ONLP_STATUS_E_UNSUPPORTED;
        break;
    }

    return rv;
}

int
onlp_sfpi_denit(void)
{
    return ONLP_STATUS_OK;
}
