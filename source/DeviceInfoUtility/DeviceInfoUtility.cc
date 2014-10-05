/**
 * @file DeviceInfoUtility/DeviceInfoUtility.cc
 *
 * Copyright 2013
 * Carnegie Robotics, LLC
 * 4501 Hatfield Street, Pittsburgh, PA 15201
 * http://www.carnegierobotics.com
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Carnegie Robotics, LLC nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CARNEGIE ROBOTICS, LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Significant history (date, user, job code, action):
 *   2013-05-22, ekratzer@carnegierobotics.com, PR1044, Created file.
 **/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#include <LibMultiSense/MultiSenseChannel.hh>

using namespace crl::multisense;

namespace {  // anonymous

void usage(const char *programNameP) 
{
    fprintf(stderr, "USAGE: %s [<options>]\n", programNameP);
    fprintf(stderr, "Where <options> are:\n");
    fprintf(stderr, "\t-a <ip_address>    : ip address (default=10.66.171.21)\n");
    fprintf(stderr, "\t-k <passphrase>    : passphrase for setting device info\n");
    fprintf(stderr, "\t-s <file_name>     : set device info from file\n");
    fprintf(stderr, "\t-q                 : query device info (default)\n");
    fprintf(stderr, "\t-y                 : disable confirmation prompt\n");
    
    exit(-1);
}

//
// Pass over any whitespace

const char *skipSpace(const char *strP)
{
    if (strP)
        while (isspace(*strP))
            strP++;
    return strP;
}

//
// Dump device info contents to a file in format that can
// be re-parsed by this utility

void printDeviceInfo(const system::DeviceInfo& info,
                     FILE*                     fP=stdout)
{
     fprintf(fP, "deviceName: %s\n", info.name.c_str());
     fprintf(fP, "buildDate: %s\n", info.buildDate.c_str());
     fprintf(fP, "serialNumber: %s\n", info.serialNumber.c_str());
     fprintf(fP, "hardwareRevision: %d\n\n", info.hardwareRevision);
          
     fprintf(fP, "imagerName: %s\n", info.imagerName.c_str());
     fprintf(fP, "imagerType: %d\n", info.imagerType);
     fprintf(fP, "imagerWidth: %d\n", info.imagerWidth);
     fprintf(fP, "imagerHeight: %d\n\n", info.imagerHeight);
     
     fprintf(fP, "lensName: %s\n", info.lensName.c_str());
     fprintf(fP, "lensType: %d\n", info.lensType);
     fprintf(fP, "nominalBaseline: %f\n", info.nominalBaseline);
     fprintf(fP, "nominalFocalLength: %f\n", info.nominalFocalLength);
     fprintf(fP, "nominalRelativeAperture: %f\n\n", info.nominalRelativeAperture);
     
     fprintf(fP, "lightingType: %d\n", info.lightingType);
     fprintf(fP, "numberOfLights: %d\n\n", info.numberOfLights);
     
     fprintf(fP, "laserName: %s\n", info.laserName.c_str());
     fprintf(fP, "laserType: %d\n\n", info.laserType);
     
     fprintf(fP, "motorName: %s\n", info.motorName.c_str());
     fprintf(fP, "motorType: %d\n", info.motorType);
     fprintf(fP, "motorGearReduction: %f\n\n", info.motorGearReduction);

     for(uint32_t i=0; i<info.pcbs.size(); i++)
         fprintf(fP, "pcb: %d %s\n", info.pcbs[i].revision,
                 info.pcbs[i].name.c_str());
}

//
// Parse a file with device information

bool parseFile(const std::string& fileName,
               system::DeviceInfo& info)
{
    FILE *fP = fopen(fileName.c_str(), "r");
    if (NULL == fP) {
        fprintf(stderr, "fopen(\"%s\") failed: %s",
                fileName.c_str(), strerror(errno));
        return false;
    }

    while (!feof(fP)) {

        char  lineP[512] = {0};
        char  tempP[512] = {0};
        int   tempi;
        float tempf;

        if (NULL == fgets(lineP, 512, fP))
            break;

        const char *s = skipSpace(lineP);
        if (!s || '#' == *s || '\0' == *s)
            continue;

#define CASE_STR(str_,x_)                                               \
            if (0 == strncasecmp(s, str_, strlen(str_))) {              \
                if (1 != sscanf(s, str_"%[^\n]", tempP)) {              \
                    fprintf(stderr, "malformed " str_ " %s\n",s);       \
                    return false;                                       \
                } else {                                                \
                    x_ = std::string(tempP);                            \
                    continue;                                           \
                }}                                                      \
            
#define CASE_INT(str_,x_)                                               \
            if (0 == strncasecmp(s, str_, strlen(str_))) {              \
                if (1 != sscanf(s, str_"%d\n", &tempi)) {               \
                    fprintf(stderr, "malformed " str_ " %s\n",s);       \
                    return false;                                       \
                } else {                                                \
                    x_ = tempi;                                         \
                    continue;                                           \
                }}                                                      \
            
#define CASE_FLT(str_,x_)                                               \
            if (0 == strncasecmp(s, str_, strlen(str_))) {              \
                if (1 != sscanf(s, str_"%f\n", &tempf)) {               \
                    fprintf(stderr, "malformed " str_ " %s\n",s);       \
                    return false;                                       \
                } else {                                                \
                    x_ = tempf;                                         \
                    continue;                                           \
                }}                                                      \

#define CASE_PCB(str_)                                                  \
        if (0 == strncasecmp(s, str_, strlen(str_))) {                  \
            if (2 != sscanf(s, str_"%d %[^\n]", &tempi, tempP)) {       \
                fprintf(stderr, "malformed " str_ " %s\n",s);           \
                return false;                                           \
            } else                                                      \
                

        CASE_STR("deviceName: ",              info.name);
        CASE_STR("buildDate: ",               info.buildDate);
        CASE_STR("serialNumber: ",            info.serialNumber);
        CASE_INT("hardwareRevision: ",        info.hardwareRevision);
        CASE_STR("imagerName: ",              info.imagerName);
        CASE_INT("imagerType: ",              info.imagerType);
        CASE_INT("imagerWidth: ",             info.imagerWidth);
        CASE_INT("imagerHeight: ",            info.imagerHeight);
        CASE_STR("lensName: ",                info.lensName);
        CASE_INT("lensType: ",                info.lensType);
        CASE_FLT("nominalBaseline: ",         info.nominalBaseline);
        CASE_FLT("nominalFocalLength: ",      info.nominalFocalLength);
        CASE_FLT("nominalRelativeAperture: ", info.nominalRelativeAperture);
        CASE_INT("lightingType: ",            info.lightingType);
        CASE_INT("numberOfLights: ",          info.numberOfLights);
        CASE_STR("laserName: ",               info.laserName);
        CASE_INT("laserType: ",               info.laserType);
        CASE_STR("motorName: ",               info.motorName);
        CASE_INT("motorType: ",               info.motorType);
        CASE_FLT("motorGearReduction: ",      info.motorGearReduction);
        CASE_PCB("pcb: ") {
            if (system::DeviceInfo::MAX_PCBS == info.pcbs.size()) {
                fprintf(stderr, "no room for pcb: %s %d\n", tempP, tempi);
                return false;
            } else {
                system::PcbInfo pcb;
                pcb.name     = std::string(tempP);
                pcb.revision = tempi;
                info.pcbs.push_back(pcb);
            }
            continue;
        }}
        
        fprintf(stderr, "malformed line: \"%s\"\n", s);
        return false;
    }

    fclose(fP);
    return true;
}

}; // anonymous

int main(int    argc, 
         char **argvPP)
{
    std::string ipAddress  = "10.66.171.21";
    std::string key;
    std::string fileName;
    bool        query  = false;
    bool        prompt = true;

    //
    // Parse args

    int c;

    while(-1 != (c = getopt(argc, argvPP, "a:k:s:qy")))
        switch(c) {
        case 'a': ipAddress = std::string(optarg);    break;
        case 'k': key       = std::string(optarg);    break;
        case 's': fileName  = std::string(optarg);    break;
        case 'q': query     = true;                   break;
        case 'y': prompt    = false;                  break;
        default: usage(*argvPP);                      break;
        }

    if (!fileName.empty() && key.empty()) {
        fprintf(stderr, 
                "To program device info, please also specify the device's key with '-k'\n");
        usage(*argvPP);
    }

    if (fileName.empty())
        query = true;

    //
    // Initialize communications.

    Channel *channelP = Channel::Create(ipAddress);
    if (NULL == channelP) {
	fprintf(stderr, "Failed to establish communications with \"%s\"\n",
		ipAddress.c_str());
	return -1;
    }

    //
    // Query version

    Status      status;
    VersionType version;

    status = channelP->getSensorVersion(version);
    if (Status_Ok != status) {
        fprintf(stderr, "Failed to query sensor version: %s\n", 
                Channel::statusString(status));
        goto clean_out;
    }

    //
    // Parse and send device info, if requested

    if (!fileName.empty()) {
        
        system::DeviceInfo info;
        
        if (false == parseFile(fileName, info))
            goto clean_out;
        else {

            if (prompt) {
                printDeviceInfo(info);
                fprintf(stdout, "\nReally update device information? (y/n): ");
                fflush(stdout);
                int c = getchar();
                
                if ('Y' != c && 'y' != c) {
                    fprintf(stdout, "Aborting\n");
                    goto clean_out;
                }
            }

            status = channelP->setDeviceInfo(key, info);
            if (Status_Ok != status) {
                fprintf(stderr, "Failed to set the device info: %s\n", 
                        Channel::statusString(status));
                goto clean_out;
            } else
                fprintf(stdout, "Device info updated successfully\n");
        }
    }

    //
    // Query the current device information, if requested
    
    if (query) {

        system::DeviceInfo info;

        status = channelP->getDeviceInfo(info);
        if (Status_Ok != status)
            fprintf(stderr, "Failed to query device info: %s\n", 
                    Channel::statusString(status));
        else
            printDeviceInfo(info);
    }

clean_out:

    Channel::Destroy(channelP);
    return 0;
}