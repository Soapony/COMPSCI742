#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include <string>
#include <vector>

using namespace ns3;

uint32_t nCsma = 20; //fix
int tcpSegmentSize = 1472; //fix
double simulationTime = 10; //fix
std::string p2pDataRate = "5Mbps"; //var 5*0.5=2.5Mbps/8*1000000/10/1472
std::string p2pDelay = "250ms"; //fix
std::string queueSize = "21p"; //var  0.1BDP / 2BDP
std::string CsmaDataRate = "100Mbps"; //fix
std::string CsmaDelay = "1ms"; //fix
int totalBytes = 0; //var
//std::string onOffDataRate = "100Mbps"; //fix
std::vector<double> queueUtilization;
double queueMax = 21;

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
  double tmp = (double)newValue / queueMax;
  queueUtilization.push_back(tmp);
}

int 
main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcpSegmentSize));

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  NodeContainer receiverNodes;
  receiverNodes.Add (p2pNodes.Get (1));
  receiverNodes.Create (nCsma);
  
  NodeContainer senderNodes;
  senderNodes.Add (p2pNodes.Get (0));
  senderNodes.Create (nCsma);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (p2pDataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (p2pDelay));
  pointToPoint.SetQueue("ns3::DropTailQueue","MaxSize",StringValue(queueSize));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);
  
  Ptr<NetDevice> nd = p2pDevices.Get (0);
  Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice> (nd);
  Ptr<Queue<Packet> > queue = ptpnd->GetQueue ();
  queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&DevicePacketsInQueueTrace));

  CsmaHelper receiverCsma;
  receiverCsma.SetChannelAttribute ("DataRate", StringValue (CsmaDataRate));
  receiverCsma.SetChannelAttribute ("Delay", StringValue(CsmaDelay));

  NetDeviceContainer receiverDevices;
  receiverDevices = receiverCsma.Install (receiverNodes);
  
  CsmaHelper senderCsma;
  senderCsma.SetChannelAttribute ("DataRate", StringValue (CsmaDataRate));
  senderCsma.SetChannelAttribute ("Delay", StringValue(CsmaDelay));

  NetDeviceContainer senderDevices;
  senderDevices = senderCsma.Install (senderNodes);
  
  InternetStackHelper stack;
  stack.Install (senderNodes);
  stack.Install (receiverNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer senderInterfaces;
  senderInterfaces = address.Assign (senderDevices);
  
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer receiverInterfaces;
  receiverInterfaces = address.Assign (receiverDevices);
  
  PacketSinkHelper receiver ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny(), 9));
  ApplicationContainer receiverApp;
  for(int i=0;i<20;i++){
    receiverApp.Add(receiver.Install (receiverNodes.Get (i+1)));
  }
  receiverApp.Start (Seconds (0.0));
  receiverApp.Stop (Seconds (10.0));



  OnOffHelper senderOnoff("ns3::TcpSocketFactory", Ipv4Address::GetAny());
  senderOnoff.SetAttribute("PacketSize",UintegerValue(tcpSegmentSize));
  senderOnoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  senderOnoff.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  //senderOnoff.SetAttribute("DataRate", StringValue(onOffDataRate));
  senderOnoff.SetAttribute("MaxBytes", UintegerValue(totalBytes));

  ApplicationContainer senderApps;
  for(int i=0;i<20;i++){
    InetSocketAddress rmt(receiverInterfaces.GetAddress(i+1),9);
    rmt.SetTos(0xb8);
    AddressValue receiverAddress(rmt);
    senderOnoff.SetAttribute("Remote", receiverAddress);
  
    senderApps.Add(senderOnoff.Install (senderNodes.Get(i+1)));
  }
  
  senderApps.Start (Seconds (0.0));
  senderApps.Stop (Seconds (simulationTime + 0.1));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 5));
  Simulator::Run ();
  
  //print simulation statistics
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  /*
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter){
    std::cout <<" Flow ID: "<<iter->first<<std::endl;
    std::cout << "  Tx Packets/Bytes:   " << iter->second.txPackets
            << " / " << iter->second.txBytes << std::endl;
    std::cout << "  Offered Load: " << iter->second.txBytes * 8.0 / (iter->second.timeLastTxPacket.GetSeconds () - iter->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets/Bytes:   " << iter->second.rxPackets
            << " / " << iter->second.rxBytes << std::endl;
  uint32_t packetsDroppedByQueueDisc = 0;
  uint64_t bytesDroppedByQueueDisc = 0;
  if (iter->second.packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
    {
      packetsDroppedByQueueDisc = iter->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      bytesDroppedByQueueDisc = iter->second.bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
    }
  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
            << " / " << bytesDroppedByQueueDisc << std::endl;
  std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds () - iter->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << iter->second.delaySum.GetSeconds () / iter->second.rxPackets << std::endl;
  
  }
  */
  
  Simulator::Destroy ();
  
  std::cout << std::endl << "*** Flow statistics ***" << std::endl;
  double thr = 0;
  int totalTxPacket = 0;
  int totalRxPacket = 0;
  int totalTxBytes = 0;
  int totalRxBytes = 0;
  int totalDroppedPacket = 0;
  int totalDroppedBytes = 0;
  double throughputSum = 0.0;
  double delaySum = 0.0;
  double avgGoodputSum = 0.0;
  for(int i=0;i<20;i++){
    std::cout <<" Flow ID: "<<i+1<<std::endl;
    
    std::cout << "  Tx Packets/Bytes:   " << stats[i+1].txPackets << " / " << stats[i+1].txBytes << std::endl;
    std::cout << "  Offered Load: " << stats[i+1].txBytes * 8.0 / (stats[i+1].timeLastTxPacket.GetSeconds () - stats[i+1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
    std::cout << "  Rx Packets/Bytes:   " << stats[i+1].rxPackets << " / " << stats[i+1].rxBytes << std::endl;
    
    uint32_t packetsDroppedByQueueDisc = 0;
    uint64_t bytesDroppedByQueueDisc = 0;
    if (stats[i+1].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
    {
      packetsDroppedByQueueDisc = stats[i+1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      bytesDroppedByQueueDisc = stats[i+1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
    }
    std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc << " / " << bytesDroppedByQueueDisc << std::endl;
    
    uint32_t packetsDroppedByNetDevice = 0;
    uint64_t bytesDroppedByNetDevice = 0;
    if (stats[i+1].packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
        packetsDroppedByNetDevice = stats[i+1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
        bytesDroppedByNetDevice = stats[i+1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
    std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice << " / " << bytesDroppedByNetDevice << std::endl;
    
    double throughput = stats[i+1].rxBytes * 8.0 / (stats[i+1].timeLastRxPacket.GetSeconds () - stats[i+1].timeFirstRxPacket.GetSeconds ()) / 1000000;
    std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
    
    double meanDelay = stats[i+1].delaySum.GetSeconds () / stats[i+1].rxPackets;
    std::cout << "  Mean delay:   " << meanDelay << std::endl;
    
    uint64_t totalPacketsThr = DynamicCast<PacketSink> (receiverApp.Get (i))->GetTotalRx ();
    thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
    std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
    
    std::cout << "  Average Goodput: " << thr << " Mbps" << std::endl;
    
    totalTxPacket += stats[i+1].txPackets;
    totalTxBytes += stats[i+1].txBytes;
    totalRxPacket += stats[i+1].rxPackets;
    totalRxBytes += stats[i+1].rxBytes;
    totalDroppedPacket += packetsDroppedByQueueDisc;
    totalDroppedBytes += bytesDroppedByQueueDisc;
    avgGoodputSum += thr;
    throughputSum += throughput;
    delaySum += meanDelay;
    
    std::cout<<std::endl<<std::endl;
    
  }
  std::cout <<"*** Total statistics ***"<<std::endl;
  std::cout <<"Total TxPacket "<<totalTxPacket<<std::endl;
  std::cout <<"Total TxBytes "<<totalTxBytes<<std::endl;
  std::cout <<"Total RxPacket "<<totalRxPacket<<std::endl;
  std::cout <<"Total RxBytes "<<totalRxBytes<<std::endl;
  std::cout <<"Total DroppedPacket "<<totalDroppedPacket<<std::endl;
  std::cout <<"Total DroppedBytes "<<totalDroppedBytes<<std::endl;
  std::cout <<"Avg Throughput "<<throughputSum / 20<<" Mbps"<<std::endl;
  std::cout <<"Avg Delay/RTT "<<delaySum / 20<<" / "<<delaySum / 20 * 2<<std::endl;
  std::cout <<"Avg Goodput "<<avgGoodputSum / 20<<" Mbps"<<std::endl;
  std::cout <<std::endl;
  std::cout <<"*** DropTailQueue Utilization ***"<<std::endl;
  for(std::vector<double>::iterator iter = queueUtilization.begin(); iter != queueUtilization.end(); iter++){
    std::cout<<*iter<<" ";
  }
  std::cout<<std::endl;

  return 0;
}
