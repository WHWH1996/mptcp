/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/socket.h"
/* Setup constant values */

int SPINE_COUNT = 2;
int LEAF_COUNT = 2;
int SERVER_COUNT = 12;
int WORKER_COUNT = 10; //
int OVERSUB_RATIO = 2;
int PRIOQUEUE_COUNT = 8;
int Model_chose = 0;
int TENSOR_THRESHOLD = 256*1024; // unit: B

int MINRTO = 10; //ms

std::string MODELS_NAME[10] = {"test", "vgg19", "resnet18", "alexnet", "mlp"};
int MODELS_LAYER_NUM[10] = {8, 19, 18, 8, 3};
int MODELS_LAYER_SIZE[10][100] = {
         {1024*1024, 1024*1024, 1024*1024, 1024*1024, 1024*1024, 1024*1024, 1024*1024, 1024*1024}
         ,{2*1024,36*1024,73*1024,147*1024,300*1024,600*1024,600*1024,600*1024,1100*1024,2300*1024,2300*1024,2300*1024,2300*1024,2300*1024,2300*1024,2300*1024,103*1024*1024,17*1024*1024,4*1024*1024}
         ,{9472, 36928, 36928, 36928, 36928, 73856, 147584, 147584, 147584, 295168, 590080, 590080, 590080, 1180160, 2359808, 2359808, 2359808, 523000}
         ,{34944, 614656, 885120, 1327488, 884992, 37752832, 16781312, 4097000}
         ,{1024*1024, 1024*1024, 1024*1024}
        };
int *LAYER_SIZE =  MODELS_LAYER_SIZE[0];
int LAYER_COUNT = 8;

int LEAF_SPINE_PATH_COUNT = SERVER_COUNT / OVERSUB_RATIO / SPINE_COUNT;
int BW_GBPS=10;     // Bandwidth fixed value
uint64_t LINK_CAPACITY_BASE=40000000000;

const int PORT_START = 10000;
const int PORT_END = 50000;
const int PACKET_SIZE = 1400;
const int MEASURED_RTT_US = 25;

const double startTime = 0.0;
const double endTime = 100.0;

double LOSELESS_RATIO = 0.1;

//double m1[24] = {5563950.0, 5658492.0, 5658914.0, 5689531.0, 5689908.0, 5716769.0, 5724326.0, 5726333.0, 5797737.0, 5798447.0, 6032082.0, 6032783.0, 6094262.0, 6094493.0, 6215879.0, 6219448.0, 6492843.0, 6493034.0, 6650461.0, 6651129.0, 7053700.0, 7054012.0, 7153180.0, 7153470.0};

//double m1[24] = {71638287.0, 71683247.0, 71773316.0, 71875356.0, 73817728.0, 73926779.0, 73927069.0, 74197119.0, 77086963.0, 77379637.0, 81117388.0, 81260547.0, 81480779.0, 94004196.0, 117522270.0, 117622801.0, 117738649.0, 134928614.0, 134940665.0, 135446707.0, 158944527.0, 159140886.0, 190107754.0, 190755403.0};

int ComputeBdp()
{
  int bdp = BW_GBPS * MEASURED_RTT_US * 1000 / PACKET_SIZE / 8;
  std::cout << "BDP value is " << bdp << " packets" << std::endl;
  return bdp;
}

template<typename T>
T rand_range (T min, T max)
{
  return min + ((double)max - min) * rand () / RAND_MAX;
}

const int BDP=ComputeBdp();
const int BUFFER_SIZE=240;
const int ECN_THRESH=65;

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Parameter server experiments");
//Gnuplot2dDataset queuediscDataset;

//void ReadStartTime()

void InstallTcpApplication(NodeContainer servers, int parameterSize,
                           int senderIndex, int receiverIndex, int flowid, int layerid)
{
  int tcpFlowSize = 0;
  tcpFlowSize = parameterSize;

  Ptr<Node> destServer = servers.Get (receiverIndex);
  Ptr<Ipv4> ipv4 = destServer -> GetObject<Ipv4> ();
  Ipv4InterfaceAddress destInterface = ipv4 -> GetAddress (1,0);
  Ipv4Address destAddress = destInterface.GetLocal();

  uint16_t port = rand_range(PORT_START, PORT_END);

  MpTcpBulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(destAddress, port));
  source.SetAttribute ("SendSize", UintegerValue (tcpFlowSize));
  source.SetAttribute ("MaxBytes", UintegerValue (tcpFlowSize));
  ApplicationContainer sourceApp = source.Install (servers.Get (senderIndex));
  sourceApp.Start(Seconds(startTime));
  sourceApp.Stop(Seconds(endTime));

  MpTcpPacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install (servers.Get (receiverIndex));
  sinkApp.Start (Seconds(startTime));
  sinkApp.Stop (Seconds(endTime));
  
}

/**
 * \brief a PS application follows all-to-all communication pattern
 * 
 * Important baselines 1. Real TCP; 2. Hypothetical TCP; 3. MLT; 4. TCP w/ cutoff
 * Possibly TODO: TCP with deadline
 */
void InstallAllApplications (NodeContainer servers)
{
  NS_LOG_INFO ("Install Applications");

  std::stringstream flowLayerFilename;

  flowLayerFilename << "tracerst/ring_flow_" << "mptcp" << "_" << MODELS_NAME[Model_chose] << "_bw" << BW_GBPS << "_lossrt" << LOSELESS_RATIO 
                      << "_" << SPINE_COUNT << "to" << LEAF_COUNT << "_s" << SERVER_COUNT << "_worker" << WORKER_COUNT
                      << "_o" << OVERSUB_RATIO << "_rto"<< MINRTO << ".flowlayer";

  std::ofstream fout(flowLayerFilename.str());
  NS_ASSERT(fout); //check whether the file be successfully created

  int totalServerCount = LEAF_COUNT * SERVER_COUNT;
  std::vector<int> indexVector;

  for (int senderIndex = 0; senderIndex < totalServerCount; ++senderIndex) 
  {
    if (senderIndex % SERVER_COUNT >= WORKER_COUNT) continue; // the node is not worker.

    indexVector.push_back(senderIndex);
  }
  int totalWorkerCount = indexVector.size();

  /* A RING all-reduce implementation */
  int flow_count = 0;
  std::random_shuffle(indexVector.begin(), indexVector.end());
  for (int i = 0; i < totalWorkerCount; ++i) 
  {
    int senderIndex = indexVector[i];
    int receiverIndex = indexVector[(i+1)%totalWorkerCount];
    for (int layerIndex = 0; layerIndex < LAYER_COUNT; ++layerIndex)
    {
      fout << "flowId:" << flow_count << " " << "srcIP:" << senderIndex << " dstIP:" << receiverIndex << std::endl;
      flow_count++;;
      //std::cout << "send from " << senderIndex << " to " << receiverIndex << ": " << LAYER_SIZE[layerIndex] / (WORKER_COUNT * LEAF_COUNT) << std::endl;
      InstallTcpApplication(servers, LAYER_SIZE[layerIndex],
                      senderIndex, receiverIndex,
                      flow_count, layerIndex+1);
    }
  }
  fout.close();
  std::cout << "flow number is " << flow_count << std::endl;
}

/* Utility functions */
void ConfigTcp()
{
  NS_LOG_INFO ("Enabling MPTCP");

  /*Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (4*MINRTO)));
  Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue (MilliSeconds (4*MINRTO)));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (MINRTO)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));*/
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (20)));
  //Config::SetDefault ("ns3::RttEstimator::MinRto", TimeValue (MilliSeconds (MINRTO)));


  LogComponentEnable("MpTcpSocketBase", LOG_NONE);

  Config::SetDefault("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(true));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
  Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(BUFFER_SIZE));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(MpTcpSocketBase::GetTypeId()));
  Config::SetDefault("ns3::MpTcpSocketBase::MaxSubflows", UintegerValue(8));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

  return;
}

void ConfigSpineLeafTopology(NodeContainer& spines,
                             NodeContainer& leaves,
                             NodeContainer& servers,
                             InternetStackHelper& internet,
                             PointToPointHelper& p2p,
                             Ipv4AddressHelper& ipv4)
{
  spines.Create (SPINE_COUNT);
  leaves.Create (LEAF_COUNT);
  servers.Create (SERVER_COUNT * LEAF_COUNT);
/*
  Ipv4GlobalRoutingHelper globalRoutingHelper;
  internet.SetRoutingHelper (globalRoutingHelper);
*/

  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  NS_LOG_INFO ("Configuring servers");
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
  p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(1)));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
}

void ConfigNetwork(std::vector<Ipv4Address>& leafNetworks,
                   std::vector<Ipv4Address>& serverAddresses,
                   Ipv4AddressHelper& ipv4,
                   NodeContainer& spines,
                   NodeContainer& leaves,
                   NodeContainer& servers,
                   PointToPointHelper& p2p)
{
  for (int i = 0; i < LEAF_COUNT; ++i) {
    Ipv4Address network = ipv4.NewNetwork ();
    leafNetworks[i] = network;
    
    for (int j = 0; j < SERVER_COUNT; ++j) {
      int serverIndex = i* SERVER_COUNT + j;
      NodeContainer nodeContainer = NodeContainer (leaves.Get (i),
                                                   servers.Get (serverIndex));
      p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
      p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(10)));
      p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
      NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
      Ipv4InterfaceContainer interfaceContainer = ipv4.Assign (netDeviceContainer);
      ipv4.NewNetwork();
      
      NS_LOG_INFO ("Leaf - " << i << " is connected to Server - " << j
                   << " with address " << interfaceContainer.GetAddress(0)
                   << " <-> " << interfaceContainer.GetAddress (1)
                   << " with port " << netDeviceContainer.Get (0)->GetIfIndex ()
                   << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ());

      serverAddresses [serverIndex] = interfaceContainer.GetAddress (1);

      //std::cout << serverIndex << " " << serverAddresses [serverIndex] << std::endl;
    }
    //std::cout << LEAF_SPINE_PATH_COUNT << std::endl;

    for (int k = 0; k < SPINE_COUNT; ++k) {
      for (int l = 0; l < LEAF_SPINE_PATH_COUNT; ++l) {
        NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get(k));
        p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
        p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(1)));
        p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);        
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork();
      }
    }
  }
}

void RunSimulation()
{
  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (endTime));
  Simulator::Run ();
}


/**
 * \brief Set monitor names and generate output
 */
void OutputMonitor (Ptr<FlowMonitor> flowMonitor)
{
  std::stringstream flowMonitorFilename;
  flowMonitorFilename << "tracerst/ring_flow_" << "mptcp" << "_" << MODELS_NAME[Model_chose] << "_bw" << BW_GBPS << "_lossrt" << LOSELESS_RATIO 
                      << "_" << SPINE_COUNT << "to" << LEAF_COUNT << "_s" << SERVER_COUNT << "_worker" << WORKER_COUNT
                      << "_o" << OVERSUB_RATIO << "_rto"<< MINRTO << ".xml";
  
  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
}

void SetCommandLine(CommandLine& cmd)
{
  cmd.AddValue("spine", "Total number of spines", SPINE_COUNT);
  cmd.AddValue("leaf",  "Total number of leaves", LEAF_COUNT);
  cmd.AddValue("server", "Total number of servers", SERVER_COUNT);
  cmd.AddValue("oversub", "Oversubscription ratio", OVERSUB_RATIO);
  cmd.AddValue("Model", "Index of Model dataset", Model_chose);
  cmd.AddValue("workerNum", "Number of workers in one rack", WORKER_COUNT);
  cmd.AddValue("rtoMin", "the value of rto_min", MINRTO);
  cmd.AddValue("bw", "downlink bandwidth", BW_GBPS);
}

int main(int argc, char** argv)
{
  LogComponentEnable("Parameter server experiments", LOG_NONE);
  CommandLine cmd;
  SetCommandLine(cmd);
  cmd.Parse(argc, argv);
  LEAF_SPINE_PATH_COUNT = SERVER_COUNT / OVERSUB_RATIO / SPINE_COUNT;
  ConfigTcp();
  LAYER_SIZE =  MODELS_LAYER_SIZE[Model_chose];
  LAYER_COUNT = MODELS_LAYER_NUM[Model_chose];

  LINK_CAPACITY_BASE=(uint64_t(BW_GBPS)*1000000000);
  
  NS_LOG_INFO ("Setting up spine-leaf topology");
  NodeContainer spines, leaves, servers;
  InternetStackHelper internet;
  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;
  ConfigSpineLeafTopology (spines, leaves, servers, internet,
                           p2p, ipv4);
  
  std::vector<Ipv4Address> leafNetworks (LEAF_COUNT);
  std::vector<Ipv4Address> serverAddresses (SERVER_COUNT * LEAF_COUNT);
  //std::map<std::pair<int, int>, uint32_t> leafToSpinePath;
  //std::map<std::pair<int, int>, uint32_t> spineToLeafPath;
  ConfigNetwork(leafNetworks, serverAddresses,
                ipv4, spines, leaves, servers, p2p);
 
  NS_LOG_INFO ("Populate global routing tables");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  InstallAllApplications (servers); //273066
  
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
  
  RunSimulation();
  OutputMonitor(flowMonitor);
  Simulator::Destroy();
  NS_LOG_INFO ("Stop simulation");
  
  return 0;
}
