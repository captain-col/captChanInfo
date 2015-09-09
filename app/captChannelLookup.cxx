#include <TChannelInfo.hxx>
#include <TEventContext.hxx>
#include <TGeometryId.hxx>
#include <TChannelId.hxx>
#include <TTPCChannelId.hxx>
#include <CaptGeomId.hxx>

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <ctime>
#include <string>

void usage() {
    std::cout << "Usage: capt-channel-lookup.exe <input option> <output option>"
              << std::endl
              << std::endl
              << "     -C : Translate from electronics <crate>-<card>-<chan>"
              << std::endl
              << "     -G : Translate from geometry [uvx]-number"
              << std::endl
              << "     -W : Translate from tpc wire"
              << std::endl
              << "     -c : Translate to electronics <crate>-<card>-<chan>"
              << std::endl
              << "     -g : Translate to geometry"
              << std::endl
              << "     -w : Translate to wire"
              << std::endl
              << "     -a : translate to asic"
              << std::endl;

}

int main(int argc, char** argv) {
    int runId = 4400;
    int eventId = 1;
    int partition = CP::TEventContext::kmCAPTAIN;
    std::time_t timeStamp = std::time(0);

    CP::TGeometryId referenceGeometryId;
    CP::TChannelId referenceChannelId;
    int referenceWire = -1;

    bool findGeometryId = false;
    bool findChannelId = false;
    bool findWire = false;
    bool findASIC = false;
    
    // Process the options.
    for (;;) {
        int c = getopt(argc, argv, "C:G:W:acgwh");
        if (c<0) break;
        switch (c) {
        case 'h': {
            usage();
            exit(0);
        }
        case 'C':
        {
            // Get a channel id  "m-a-c".  e.g. 1-4-11
            int crate = -1;
            int slot = -1;
            int channel = -1;
            char c;
            std::istringstream cvt(optarg);
            cvt >> crate >> c >> slot >> c >> channel ;
            referenceChannelId = CP::TTPCChannelId(crate,slot,channel);
            break;
        }
        case 'G':
        {
            // Get a geometry id  "[uvx]-<wire>".  e.g. X-231
            std::string opt(optarg);
            int plane = -1;
            int wire = -1;
            if (opt[0] == 'U' || opt[0] == 'u') {
                plane = CP::GeomId::Captain::kUPlane;
            }
            else if (opt[0] == 'V' || opt[0] == 'v') {
                plane = CP::GeomId::Captain::kVPlane;
            }
            else if (opt[0] == 'X' || opt[0] == 'x') {
                plane = CP::GeomId::Captain::kXPlane;
            }
            std::istringstream cvt(opt.substr(2));
            cvt >> wire ;
            referenceGeometryId = CP::GeomId::Captain::Wire(plane,wire);
            break;
        }
        case 'W':
        {
            // Get a wire number
            std::istringstream cvt(optarg);
            cvt >> referenceWire;
            break;
        }
        case 'a': findASIC = true; break;
        case 'c': findChannelId = true; break;
        case 'g': findGeometryId = true; break;
        case 'w': findWire = true; break;
        default:
            std::cout << "Invalid option" << std::endl;
            usage();
            exit(-1);
        }
    }

    CP::TEventContext context;
    context.SetRun(runId);
    context.SetEvent(eventId);
    context.SetPartition(partition);
    context.SetTimeStamp(timeStamp);

    CP::TChannelInfo& channelInfo = CP::TChannelInfo::Get();
    channelInfo.SetContext(context);

    if (findWire) {
        if (referenceChannelId.IsValid()) {
            int wire = channelInfo.GetWireNumber(referenceChannelId);
            std::cout << wire << std::endl;
            exit(0);
        }
        if (referenceGeometryId.IsValid()) {
            int wire = channelInfo.GetWireNumber(referenceGeometryId);
            std::cout << wire << std::endl;
            exit(0);
        }
        std::cout << "Invalid inputs" << std::endl;
        exit(-1);
    }

    if (findChannelId) {
        if (referenceWire != -1) {
            CP::TChannelId id = channelInfo.GetChannel(referenceWire);
            std::cout << id << std::endl;
            exit(0);
        }
        if (referenceGeometryId.IsValid()) {
            CP::TChannelId id = channelInfo.GetChannel(referenceGeometryId);
            std::cout << id << std::endl;
            exit(0);
        }
        std::cout << "Invalid inputs" << std::endl;
        exit(-1);
    }

    if (findGeometryId) {
        CP::TGeometryId id;
        if (referenceWire != -1) {
            id = channelInfo.GetGeometry(referenceWire);
        }
        else if (referenceChannelId.IsValid()) {
            id = channelInfo.GetGeometry(referenceChannelId);
        }
        else {
            std::cout << "Invalid inputs" << std::endl;
            exit(-1);
        }
        if (!id.IsValid()) {
            std::cout << "Invalid geometry" << std::endl;
            exit(-1);
        }
        if (!CP::GeomId::Captain::IsWire(id)) {
            std::cout << "Not a wire" << std::endl;
            exit(-1);
        }
        std::cout << "    ";
        if (CP::GeomId::Captain::IsUWire(id)) std::cout << "U";
        if (CP::GeomId::Captain::IsVWire(id)) std::cout << "V";
        if (CP::GeomId::Captain::IsXWire(id)) std::cout << "X";
        std::cout << "-" << CP::GeomId::Captain::GetWireNumber(id)
                  << std::endl;
        exit(0);
    }

    if (findASIC) {
        if (referenceWire != -1) {
            referenceChannelId = channelInfo.GetChannel(referenceWire);
        }
        else if (referenceGeometryId.IsValid()) {
            referenceChannelId = channelInfo.GetChannel(referenceGeometryId);
        }
        int motherboard = channelInfo.GetMotherboard(referenceChannelId);
        int asic = channelInfo.GetASIC(referenceChannelId);
        int asicChan = channelInfo.GetASICChannel(referenceChannelId);
        std::cout << "    MB: " << motherboard
                  << " ASIC: " << asic
                  << " Chan: " << asicChan << std::endl;
        
    }

}
