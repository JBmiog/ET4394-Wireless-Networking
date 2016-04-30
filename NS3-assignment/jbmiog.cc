/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//
// Default network topology includes some number of AP nodes specified by
// the variable nWifis (defaults to two).  Off of each AP node, there are some
// number of STA nodes specified by the variable nStas (defaults to two).
// Each AP talks to its associated STA nodes.  There are bridge net devices
// on each AP node that bridge the whole thing into one network.
//
// Enabling debug while running the script will enable a single run
// simulation, otherwise it'll go through the pre-defined runs
// and output data dat can be piped into a csv file. with &> "...".txt
//
//      +-----+      +-----+            +-----+      +-----+
//      | STA |      | STA |            | STA |      | STA | 
//      +-----+      +-----+            +-----+      +-----+
//    192.168.0.2  192.168.0.3        192.168.0.5  192.168.0.6
//      --------     --------           --------     --------
//      WIFI STA     WIFI STA           WIFI STA     WIFI STA
//      --------     --------           --------     --------
//        ((*))       ((*))       |      ((*))        ((*))
//                                |
//              ((*))             |             ((*))
//             -------                         -------
//             WIFI AP   CSMA ========= CSMA   WIFI AP  =============== k bridges with nStas
//             -------   ----           ----   -------
//             ##############           ##############
//                 BRIDGE                   BRIDGE
//             ##############           ############## 
//               192.168.0.1              192.168.0.4
//               +---------+              +---------+
//               | AP Node |              | AP Node |
//               +---------+              +---------+
//

#include <stdint.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../build/ns3/address.h"
#include "../build/ns3/application-container.h"
#include "../build/ns3/boolean.h"
#include "../build/ns3/bridge-helper.h"
#include "../build/ns3/command-line.h"
#include "../build/ns3/csma-helper.h"
#include "../build/ns3/data-rate.h"
#include "../build/ns3/double.h"
#include "../build/ns3/flow-classifier.h"
#include "../build/ns3/flow-monitor.h"
#include "../build/ns3/flow-monitor-helper.h"
#include "../build/ns3/inet-socket-address.h"
#include "../build/ns3/internet-stack-helper.h"
#include "../build/ns3/ipv4-address.h"
#include "../build/ns3/ipv4-address-helper.h"
#include "../build/ns3/ipv4-flow-classifier.h"
#include "../build/ns3/ipv4-interface-container.h"
#include "../build/ns3/log.h"
#include "../build/ns3/mobility-helper.h"
#include "../build/ns3/net-device-container.h"
#include "../build/ns3/node-container.h"
#include "../build/ns3/nqos-wifi-mac-helper.h"
#include "../build/ns3/nstime.h"
#include "../build/ns3/on-off-helper.h"
#include "../build/ns3/ptr.h"
#include "../build/ns3/rectangle.h"
#include "../build/ns3/simulator.h"
#include "../build/ns3/ssid.h"
#include "../build/ns3/string.h"
#include "../build/ns3/trace-helper.h"
#include "../build/ns3/uinteger.h"
#include "../build/ns3/wifi-helper.h"
#include "../build/ns3/wifi-phy-standard.h"
#include "../build/ns3/yans-wifi-helper.h"

using namespace ns3;

void simulator(uint32_t nWifis, uint32_t nStas, bool writeMobility, bool debug, DataRate dataRate, uint32_t packetSize){
	uint32_t totalNNodes =  nWifis * nStas;
	uint32_t totalNFlows = totalNNodes;

	NodeContainer backboneNodes;
	NetDeviceContainer backboneDevices;
	Ipv4InterfaceContainer backboneInterfaces;
	std::vector<NodeContainer> staNodes;
	std::vector<NetDeviceContainer> staDevices;
	std::vector<NetDeviceContainer> apDevices;
	std::vector<Ipv4InterfaceContainer> staInterfaces;
	std::vector<Ipv4InterfaceContainer> apInterfaces;

	InternetStackHelper stack;
	CsmaHelper csma;
	Ipv4AddressHelper ip;
	ip.SetBase ("192.168.0.0", "255.255.255.0");

	backboneNodes.Create (nWifis);
	stack.Install (backboneNodes);

	backboneDevices = csma.Install (backboneNodes);

	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

	for (uint32_t i = 0; i < nWifis; ++i){
		// calculate ssid for wifi subnetwork
		std::ostringstream oss;
		oss << "wifi-default-" << i;
		Ssid ssid = Ssid (oss.str ());

		NodeContainer sta;
		NetDeviceContainer staDev;
		NetDeviceContainer apDev;
		Ipv4InterfaceContainer staInterface;
		Ipv4InterfaceContainer apInterface;
		MobilityHelper mobility;
		BridgeHelper bridge;
		WifiHelper wifi = WifiHelper::Default ();
		wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

		NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();

		YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
		wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
		wifiPhy.SetChannel (wifiChannel.Create ());

		sta.Create (nStas);
		mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
				"MinX", DoubleValue (0.0),
				"MinY", DoubleValue (0.0),
				"DeltaX", DoubleValue (5.0),
				"DeltaY", DoubleValue (5.0),
				"GridWidth", UintegerValue (1),
				"LayoutType", StringValue ("RowFirst"));

		// setup the AP.
		mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
		mobility.Install (backboneNodes.Get (i));
		wifiMac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid));
		apDev = wifi.Install (wifiPhy, wifiMac, backboneNodes.Get (i));

		NetDeviceContainer bridgeDev;
		bridgeDev = bridge.Install (backboneNodes.Get (i), NetDeviceContainer (apDev, backboneDevices.Get (i)));

		apInterface = ip.Assign (bridgeDev);

		stack.Install (sta);

		mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
				"Bounds", RectangleValue (Rectangle (-20, 20, -20, 20)),
				"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=4]"),
				"Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));

		mobility.Install (sta);

		wifiMac.SetType ("ns3::StaWifiMac",
				"Ssid", SsidValue (ssid),
				"ActiveProbing", BooleanValue (false));
		staDev = wifi.Install (wifiPhy, wifiMac, sta);
		staInterface = ip.Assign (staDev);

		staNodes.push_back (sta);
		apDevices.push_back (apDev);
		apInterfaces.push_back (apInterface);
		staDevices.push_back (staDev);
		staInterfaces.push_back (staInterface);

		Ipv4Address addr;
		uint32_t nStaNodes = sta.GetN();
		if(debug){
			for(uint32_t j = 0 ; j < nStaNodes; j++) {
				addr = staInterface.GetAddress(j);
				std::cout << " Node " << i << "." << j << "\t "<< "IP Address "<<addr << std::endl;
			}
		}
	}

	if(debug){
		Ipv4Address addr;
		std::cout << "wifi ap node addresses.." << "\n";
		uint32_t nApNodes = backboneNodes.GetN();
		for(uint32_t i = 0 ; i < nApNodes; i++) {
			addr = apInterfaces[i].GetAddress(0);
			std::cout << " Node " << i << "\t "<< "IP Address "<< addr << std::endl;
		}
	}

	Address dest;
	std::string protocol;

	dest = InetSocketAddress (apInterfaces[1].GetAddress (0), 1025);
	protocol = "ns3::UdpSocketFactory";

	OnOffHelper onoff = OnOffHelper (protocol, dest);
	onoff.SetConstantRate (DataRate (dataRate), packetSize);

	for(uint32_t i = 0; i< nWifis; i++){
		ApplicationContainer apps = onoff.Install (staNodes[i]);
		apps.Start (Seconds (3.0));
		apps.Stop (Seconds (5));
	}

	//Generate the Pcap files to look into the traffic using wireshark
	//wifiPhy.EnablePcap ("wifi-wired-bridging", apDevices[0]);
	//wifiPhy.EnablePcap ("wifi-wired-bridging", apDevices[1]);

	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();

	if (writeMobility)
	{
		AsciiTraceHelper ascii;
		MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("wifi-wired-bridging.mob"));
	}

	Simulator::Stop (Seconds (6.0));
	Simulator::Run ();

	// credits to Rizqi Hersyandika for Flow Monitor example.
	monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

	float throughput[totalNFlows];

	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		if(debug){
			std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			std::cout << " Tx Bytes: " << i->second.txBytes << "\n";
			std::cout << " Rx Bytes: " << i->second.rxBytes << "\n";
			std::cout << " Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024 << " kbps\n";
			std::cout << " Delay : " << i->second.delaySum / i->second.rxPackets << "\n";
		}
		throughput[i->first] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024;
	}
	Simulator::Destroy ();

	float totalThroughPut = 0;
	for (uint32_t i = 1; i <= (totalNFlows); i++){
		totalThroughPut += throughput[i];
	}
	if(debug){
		std::cout << "average throughput of system = " << "**" << totalThroughPut / totalNFlows << "**" <<  "kbps" <<"\n";
	}
	std::cout <<  nWifis << " " << nStas*nWifis << " " << nStas << " " <<  totalThroughPut / totalNFlows << " " << dataRate << " " << packetSize << "\n";
}

int main (int argc, char *argv[])
{

	Time::SetResolution (Time::NS);
	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
	LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

	uint32_t nWifis = 2;
	uint32_t nStas = 2;
	bool debug = false;
	bool writeMobility = false;
	DataRate dataRate = 5000*1000;
	uint32_t packetSize = 512;

	CommandLine cmd;
	cmd.AddValue ("nWifis", "Number of wifi networks", nWifis);
	cmd.AddValue ("nStas", "Number of stations per wifi network", nStas);
	cmd.AddValue ("debug", "print debug info or use settings to print to csv file", debug);
	cmd.AddValue ("writeMobility", "Write mobility trace", writeMobility);
	cmd.AddValue ("dataRate", "set the dataRate in bps", dataRate);
	cmd.AddValue ("packetSize", "set the packetsize in byte", packetSize);
	cmd.Parse (argc, argv);
	if(debug){
		simulator(nWifis, nStas, writeMobility, debug, dataRate, packetSize);
	} else {
		//run simulations for the througput vs. node per access point, for different datarates
		//mind that there are as many flows as there are stations
		std::cout <<  "througput_vs_node/AP"<< "\n";
		std::cout <<  "nWifis" << " " << "nStas*nWifis" << " " << "nStas" << " " <<  "averageThroughput" << " " << "dataRate" <<"packetsize"<< "\n";
		simulator(30, 1, writeMobility, debug, 11000*1000, packetSize);

		simulator(15, 2, writeMobility, debug, 11000*1000, packetSize);
		simulator(10, 3, writeMobility, debug, 11000*1000, packetSize);
		simulator(6, 5,  writeMobility, debug, 11000*1000, packetSize);
		simulator(5, 6,  writeMobility, debug, 11000*1000, packetSize);
		simulator(3, 10,  writeMobility, debug, 11000*1000, packetSize);
		simulator(2, 15,  writeMobility, debug, 11000*1000, packetSize);

		simulator(30, 1, writeMobility, debug, 5500*1000, packetSize);
		simulator(15, 2, writeMobility, debug, 5500*1000, packetSize);
		simulator(10, 3, writeMobility, debug, 5500*1000, packetSize);
		simulator(6, 5,  writeMobility, debug, 5500*1000, packetSize);
		simulator(5, 6,  writeMobility, debug, 5500*1000, packetSize);
		simulator(3, 10,  writeMobility, debug, 5500*1000, packetSize);
		simulator(2, 15,  writeMobility, debug, 5500*1000, packetSize);

		simulator(30, 1, writeMobility, debug, 2000*1000, packetSize);
		simulator(15, 2, writeMobility, debug, 2000*1000, packetSize);
		simulator(10, 3, writeMobility, debug, 2000*1000, packetSize);
		simulator(6, 5,  writeMobility, debug, 2000*1000, packetSize);
		simulator(5, 6,  writeMobility, debug, 2000*1000, packetSize);
		simulator(3, 10,  writeMobility, debug, 2000*1000, packetSize);
		simulator(2, 15,  writeMobility, debug, 2000*1000, packetSize);

		simulator(30, 1, writeMobility, debug, 1000*1000, packetSize);
		simulator(15, 2, writeMobility, debug, 1000*1000, packetSize);
		simulator(10, 3, writeMobility, debug, 1000*1000, packetSize);
		simulator(6, 5,  writeMobility, debug, 1000*1000, packetSize);
		simulator(5, 6,  writeMobility, debug, 1000*1000, packetSize);
		simulator(3, 10,  writeMobility, debug, 1000*1000, packetSize);
		simulator(2, 15,  writeMobility, debug, 1000*1000, packetSize);

		//run the simulation with different packet sizes,
		std::cout <<  "payload_vs_node/AP"<< "\n";
		std::cout <<  "nWifis" << " " << "nStas*nWifis" << " " << "nStas" << " " <<  "averageThroughput" << " " << "dataRate" << "packetsize"<<"\n";
		simulator(30, 1, writeMobility, debug, 11000*1000, 1472);
		simulator(15, 2, writeMobility, debug, 11000*1000, 1472);
		simulator(10, 3, writeMobility, debug, 11000*1000, 1472);
		simulator(6, 5,  writeMobility, debug, 11000*1000, 1472);
		simulator(5, 6,  writeMobility, debug, 11000*1000, 1472);
		simulator(3, 10,  writeMobility, debug, 11000*1000, 1472);
		simulator(2, 15,  writeMobility, debug, 11000*1000, 1472);

		simulator(30, 1, writeMobility, debug, 11000*1000, 1000);
		simulator(15, 2, writeMobility, debug, 11000*1000, 1000);
		simulator(10, 3, writeMobility, debug, 11000*1000, 1000);
		simulator(6, 5,  writeMobility, debug, 11000*1000, 1000);
		simulator(5, 6,  writeMobility, debug, 11000*1000, 1000);
		simulator(3, 10,  writeMobility, debug, 11000*1000, 1000);
		simulator(2, 15,  writeMobility, debug, 11000*1000, 1000);

		simulator(30, 1, writeMobility, debug, 11000*1000, 500);
		simulator(15, 2, writeMobility, debug, 11000*1000, 500);
		simulator(10, 3, writeMobility, debug, 11000*1000, 500);
		simulator(6, 5,  writeMobility, debug, 11000*1000, 500);
		simulator(5, 6,  writeMobility, debug, 11000*1000, 500);
		simulator(3, 10,  writeMobility, debug, 11000*1000, 500);
		simulator(2, 15,  writeMobility, debug, 11000*1000, 500);
	}
}
