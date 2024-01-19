/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * NIST-developed software is provided by NIST as a public service. You may
 * use, copy and distribute copies of the software in any medium, provided that
 * you keep intact this entire notice. You may improve, modify and create
 * derivative works of the software or any portion of the software, and you may
 * copy and distribute such modifications or works. Modified works should carry
 * a notice stating that you changed the software and should note the date and
 * nature of any such change. Please explicitly acknowledge the National
 * Institute of Standards and Technology as the source of the software.
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES NO
 * WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY OPERATION OF
 * LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT AND DATA ACCURACY. NIST
 * NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST
 * DOES NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE
 * SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE
 * CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
 *
 * You are solely responsible for determining the appropriateness of using and
 * distributing the software and you assume all risks associated with its use,
 * including but not limited to the risks and costs of program errors,
 * compliance with applicable laws, damage to or loss of data, programs or
 * equipment, and the unavailability or interruption of operation. This
 * software is not intended to be used in any situation where a failure could
 * cause risk of injury or damage to property. The software developed by NIST
 * employees is not subject to copyright protection within the United States.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/oran-module.h"
#include "ns3/point-to-point-module.h"

//Biblioteca
#include <fstream>
#include <vector>


#include <math.h>
//NS3
#include "ns3/ipv4-address.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/log.h"

using namespace ns3;

const static float ENB_HEIGHT = 20;

//Variavel
uint32_t numUEs = 50;

// Intervalo de coleta das métricas
Time management_interval = Seconds(10);

// Vetor de ips de UEs
std::vector<Ipv4Address> user_ip;

// Vetores com as métricas mais recentes de cada UE
std::vector<double> user_delay;
std::vector<double> user_jitter;
std::vector<double> user_throughput;
std::vector<double> user_pdr;


static std::string s_trafficTraceFile = "traffic-trace.tr";
static std::string s_positionTraceFile = "position-trace.tr";
static std::string s_handoverTraceFile = "handover-trace.tr";

// Function that will save the traces of RX'd packets
void
RxTrace(Ptr<const Packet> p, const Address& from, const Address& to)
{
    uint16_t ueId = (InetSocketAddress::ConvertFrom(to).GetPort() / 1000);

    std::ofstream rxOutFile(s_trafficTraceFile, std::ios_base::app);
    rxOutFile << Simulator::Now().GetSeconds() << " " << ueId << " RX " << p->GetSize()
              << std::endl;
}

// Function that will save the traces of TX'd packets
void
TxTrace(Ptr<const Packet> p, const Address& from, const Address& to)
{
    uint16_t ueId = (InetSocketAddress::ConvertFrom(to).GetPort() / 1000);

    std::ofstream rxOutFile(s_trafficTraceFile, std::ios_base::app);
    rxOutFile << Simulator::Now().GetSeconds() << " " << ueId << " TX " << p->GetSize()
              << std::endl;
}

// Função ajudante que retorna o id do UE
// associado a determinado ip
int get_user_id_from_ipv4(Ipv4Address ip)
{

  for (uint32_t i = 0; i < numUEs; i++)
  {
    if (user_ip[i] == ip)
    {
      return i;
    }
  }
  return -1;
}

// Função que calcula as métricas de qos periodicamente
// usando o FlowMonitor
void ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon)
{
  // count lost packets
  flowMon->CheckForLostPackets();

  uint32_t LostPacketsum = 0;
  float PDR, PLR, Delay, Jitter, Throughput;
  auto flowStats = flowMon->GetFlowStats();

  // Trocar pela subrede dos UEs
  auto ue_network = Ipv4Address("7.0.0.0");
  auto ue_network_mask = Ipv4Mask("255.0.0.0");

  Ptr<Ipv4FlowClassifier> classing =
      DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());

	//cell_throughput.assign(numCells, 0);
  for (auto stats : flowStats)
  {
    // disconsider old flows by checking if last packet was sent or received
    // before this round management interval
    if (Simulator::Now() - stats.second.timeLastTxPacket > management_interval &&
		Simulator::Now() - stats.second.timeLastRxPacket > management_interval)
    {
      continue;
    }

    // find flow characteristics
    Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats.first);

	if(!ue_network_mask.IsMatch(ue_network, fiveTuple.destinationAddress))
		continue;

    PDR = (double)(100 * stats.second.rxPackets) / (stats.second.txPackets);
    LostPacketsum = (double)(stats.second.txPackets) - (stats.second.rxPackets);
    PLR = (double)(LostPacketsum * 100) / stats.second.txPackets;
    Delay = (stats.second.delaySum.GetSeconds()) / (stats.second.rxPackets);
	Jitter = (stats.second.jitterSum.GetSeconds()) / (stats.second.rxPackets - 1);
    Throughput = stats.second.rxBytes * 8.0 /
                 (stats.second.timeLastRxPacket.GetSeconds() -
                  stats.second.timeFirstTxPacket.GetSeconds()) /
                 1024 / 1024;

    std::cout << "Flow ID     : " << stats.first << " ; "
              << fiveTuple.sourceAddress << " -----> "
              << fiveTuple.destinationAddress << std::endl;
    std::cout << "Tx Packets = " << stats.second.txPackets << std::endl;
    std::cout << "Rx Packets = " << stats.second.rxPackets << std::endl;
    std::cout << "Lost Packets = "
              << (stats.second.txPackets) - (stats.second.rxPackets)
              << std::endl;
    std::cout << "Packets Delivery Ratio (PDR) = " << PDR << "%" << std::endl;
    std::cout << "Packets Lost Ratio (PLR) = " << PLR << "%" << std::endl;
    std::cout << "Delay = " << Delay << " Seconds" << std::endl;
    std::cout << "Total Duration    : "
              << stats.second.timeLastRxPacket.GetSeconds() -
                     stats.second.timeFirstTxPacket.GetSeconds()
              << " Seconds" << std::endl;
    std::cout << "Last Received Packet  : "
              << stats.second.timeLastRxPacket.GetSeconds() << " Seconds"
              << std::endl;
    std::cout << "Throughput: " << Throughput << " mbps" << std::endl;
    std::cout << "Throughput in bytes: " << Throughput * 1024 * 1024 / 8 << " Bps" << std::endl;
    // receiving node, used to catch user downlink traffic
    std::cout << "target node = " << get_user_id_from_ipv4(
            fiveTuple.destinationAddress) << std::endl;
    std::cout << "-------------------------------------------------------------"
                 "--------------"
              << std::endl;

    // received id will be -1 in case it is not a mobile user
    int receiver_id = get_user_id_from_ipv4(fiveTuple.destinationAddress);
    if (receiver_id != -1)
    {
      user_delay[receiver_id] = Delay;
      user_jitter[receiver_id] = Jitter;
      user_throughput[receiver_id] = Throughput;
      user_pdr[receiver_id] = PDR;
      //cell_throughput[getCellId(receiver_id)] += Throughput;
    }
  }

	std::ofstream qos_vs_time;
	qos_vs_time.open("qos-vs-time.txt", std::ofstream::out | std::ofstream::app);
  for(uint32_t ue=0; ue < numUEs; ++ue)
  {
	qos_vs_time << Simulator::Now().GetSeconds() << ","
		<< ue << ","
		//<< user_requests[ue].get_name() << ","
		<< user_delay[ue] << ","
		<< user_jitter[ue] << ","
		<< user_throughput[ue] << ","
		<< user_pdr[ue] << std::endl;
  }

  // schedule itself in 1sec
  Simulator::Schedule(management_interval, ThroughputMonitor, fmhelper,
                      flowMon);
}


// Trace each node's location
void
TracePositions(NodeContainer nodes)
{
    std::ofstream posOutFile(s_positionTraceFile, std::ios_base::app);

    posOutFile << Simulator::Now().GetSeconds();
    for (uint32_t i = 0; i < nodes.GetN(); i++)
    {
        Vector pos = nodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        posOutFile << " " << pos.x << " " << pos.y;
    }
    posOutFile << std::endl;

    Simulator::Schedule(Seconds(1), &TracePositions, nodes);
}

void
NotifyHandoverEndOkEnb(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
    std::ofstream hoOutFile(s_handoverTraceFile, std::ios_base::app);
    hoOutFile << Simulator::Now().GetSeconds() << " " << imsi << " " << cellid << " " << rnti
              << std::endl;
}

NS_LOG_COMPONENT_DEFINE("OranLte2LteMlHandoverSimulation");

void install_mobility(NodeContainer staticNodes, NodeContainer mcNodes, NodeContainer ueNodes)
{
	Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
	allocator->Add (Vector(0, 0, 0));
	MobilityHelper staticNodesHelper;
	staticNodesHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	staticNodesHelper.SetPositionAllocator (allocator);
	staticNodesHelper.Install(staticNodes);

    // Install Mobility Model for eNBs
	Ptr<ListPositionAllocator> mcPosition = CreateObject<ListPositionAllocator> ();
	mcPosition->Add (Vector(-400, 230, ENB_HEIGHT));
	mcPosition->Add (Vector(400, 230, ENB_HEIGHT));
	mcPosition->Add (Vector(0, -462, ENB_HEIGHT));
	MobilityHelper mcHelper;
	mcHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mcHelper.SetPositionAllocator(mcPosition);
	mcHelper.Install(mcNodes);

    // Install Mobility Model for UEs
	MobilityHelper ueHelper;
	ueHelper.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
								"Bounds", StringValue ("-800|800|-862|630"),
								"Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.5]"),
								"Pause", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=6.0]"));
	ueHelper.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
								 "X", StringValue ("ns3::UniformRandomVariable[Min=-790.0|Max=790.0]"),
								 "Y", StringValue ("ns3::UniformRandomVariable[Min=-852.0|Max=620.0]"));
	ueHelper.Install (ueNodes);
}

int
main(int argc, char* argv[])
{
    bool verbose = false;
    bool useOran = true;
    bool useOnnx = false;
    bool useTorch = false;
    bool useDistance = false;
    double lmQueryInterval = 1;
    double txDelay = 0;
    std::string handoverAlgorithm = "ns3::NoOpHandoverAlgorithm";
    Time simTime = Seconds(100);
    std::string dbFileName = "oran-repository.db";

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable printing SQL queries results", verbose);
    cmd.AddValue("use-oran", "Indicates whether ORAN should be used or not", useOran);
    cmd.AddValue("use-onnx-lm", "Indicates whether the ONNX LM should be used or not", useOnnx);
    cmd.AddValue("use-torch-lm",
                 "Indicates whether the PyTorch LM should be used or not",
                 useTorch);
    cmd.AddValue("use-distance-lm",
                 "Indicates whether the distance LM should be used or not",
                 useDistance);
    cmd.AddValue("sim-time", "The duration for which traffic should flow", simTime);
    cmd.AddValue("lm-query-interval", "The LM query interval", lmQueryInterval);
    cmd.AddValue("tx-delay", "The E2 termiantor's transmission delay", txDelay);
    cmd.AddValue("handover-algorithm",
                 "Specify which handover algorithm to use",
                 handoverAlgorithm);
    cmd.AddValue("db-file", "Specify the DB file to create", dbFileName);
    cmd.AddValue("traffic-trace-file",
                 "Specify the traffic trace file to create",
                 s_trafficTraceFile);
    cmd.AddValue("position-trace-file",
                 "Specify the position trace file to create",
                 s_positionTraceFile);
    cmd.AddValue("handover-trace-file",
                 "Specify the handover trace file to create",
                 s_handoverTraceFile);
	cmd.AddValue("num-ues", "Number of UEs", numUEs);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(useOran == false && (useOnnx || useTorch || useDistance),
                    "Cannot use ML LM or distance LM without enabling O-RAN.");
    NS_ABORT_MSG_IF((useOnnx + useTorch + useDistance) > 1,
                    "Cannot use more than one LM simultaneously.");
    NS_ABORT_MSG_IF(handoverAlgorithm != "ns3::NoOpHandoverAlgorithm" &&
                        (useOnnx || useTorch || useDistance),
                    "Cannot use non-noop handover algorithm with ML LM or distance LM.");

    // Increase the buffer size to accomodate the application demand
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(1000 * 1024));
    // Disabled to prevent the automatic cell reselection when signal quality is bad.
    Config::SetDefault("ns3::LteUePhy::EnableRlfDetection", BooleanValue(false));

    // Configure the LTE parameters (pathloss, bandwidth, scheduler)
	Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(43));
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::Cost231PropagationLossModel"));
    lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(100));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(50));
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");
    lteHelper->SetSchedulerAttribute("HarqEnabled", BooleanValue(true));
    lteHelper->SetHandoverAlgorithmType(handoverAlgorithm);
	// Modo de transmissão (SISO [0], Diversity [1], MIMO [2])
	Config::SetDefault("ns3::LteEnbRrc::DefaultTransmissionMode", UintegerValue(2));

    // Deploy the EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a single remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // IP configuration
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(65000));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Create eNB and UE
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(3);
    ueNodes.Create(numUEs);

	install_mobility(remoteHostContainer, enbNodes, ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

	lteHelper->AttachToClosestEnb(ueLteDevs, enbLteDevs);

    lteHelper->AddX2Interface(enbNodes);

    // Install and start applications on UEs and remote host
    uint16_t basePort = 1000;
    ApplicationContainer remoteApps;
    ApplicationContainer ueApps;

    Ptr<RandomVariableStream> onTimeRv = CreateObject<UniformRandomVariable>();
    onTimeRv->SetAttribute("Min", DoubleValue(1.0));
    onTimeRv->SetAttribute("Max", DoubleValue(5.0));
    Ptr<RandomVariableStream> offTimeRv = CreateObject<UniformRandomVariable>();
    offTimeRv->SetAttribute("Min", DoubleValue(1.0));
    offTimeRv->SetAttribute("Max", DoubleValue(5.0));

    for (uint16_t i = 0; i < ueNodes.GetN(); i++)
    {
        uint16_t port = basePort * (i + 1);

        PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), port));
        ueApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(i)));
        // Enable the tracing of RX packets
        ueApps.Get(i)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxTrace));

        Ptr<OnOffApplication> streamingServer = CreateObject<OnOffApplication>();
        remoteApps.Add(streamingServer);
        // Attributes
        streamingServer->SetAttribute(
            "Remote",
            AddressValue(InetSocketAddress(ueIpIface.GetAddress(i), port)));
        streamingServer->SetAttribute("DataRate", DataRateValue(DataRate("3000000bps")));
        streamingServer->SetAttribute("PacketSize", UintegerValue(1500));
        streamingServer->SetAttribute("OnTime", PointerValue(onTimeRv));
        streamingServer->SetAttribute("OffTime", PointerValue(offTimeRv));

        remoteHost->AddApplication(streamingServer);
        streamingServer->TraceConnectWithoutContext("TxWithAddresses", MakeCallback(&TxTrace));
    }

    // Inidcate when to start streaming
    remoteApps.Start(Seconds(2));
    // Indicate when to stop streaming
    remoteApps.Stop(simTime + Seconds(10));

    // UE applications start listening
    ueApps.Start(Seconds(1));
    // UE applications stop listening
    ueApps.Stop(simTime + Seconds(15));

    // ORAN BEGIN
    if (useOran == true)
    {
        if (!dbFileName.empty())
        {
            std::remove(dbFileName.c_str());
        }

        TypeId defaultLmTid = TypeId::LookupByName("ns3::OranLmNoop");

        Ptr<OranLm> defaultLm = nullptr;
        Ptr<OranDataRepository> dataRepository = CreateObject<OranDataRepositorySqlite>();
        Ptr<OranCmm> cmm = CreateObject<OranCmmHandover>();
        Ptr<OranNearRtRic> nearRtRic = CreateObject<OranNearRtRic>();
        Ptr<OranNearRtRicE2Terminator> nearRtRicE2Terminator =
            CreateObject<OranNearRtRicE2Terminator>();

        if (useOnnx == true)
        {
            NS_ABORT_MSG_IF(
                !TypeId::LookupByNameFailSafe("ns3::OranLmLte2LteOnnxHandover", &defaultLmTid),
                "ONNX LM not found. Were the ONNX headers and libraries found during the config "
                "operation?");
        }
        else if (useTorch == true)
        {
            NS_ABORT_MSG_IF(
                !TypeId::LookupByNameFailSafe("ns3::OranLmLte2LteTorchHandover", &defaultLmTid),
                "Torch LM not found. Were the Torch headers and libraries found during the config "
                "operation?");
        }
        else if (useDistance == true)
        {
            defaultLmTid = TypeId::LookupByName("ns3::OranLmLte2LteDistanceHandover");
        }

        ObjectFactory defaultLmFactory;
        defaultLmFactory.SetTypeId(defaultLmTid);
        defaultLm = defaultLmFactory.Create<OranLm>();

        dataRepository->SetAttribute("DatabaseFile", StringValue(dbFileName));
        defaultLm->SetAttribute("Verbose", BooleanValue(verbose));
        defaultLm->SetAttribute("NearRtRic", PointerValue(nearRtRic));

        cmm->SetAttribute("NearRtRic", PointerValue(nearRtRic));

        nearRtRicE2Terminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
        nearRtRicE2Terminator->SetAttribute("DataRepository", PointerValue(dataRepository));
        nearRtRicE2Terminator->SetAttribute(
            "TransmissionDelayRv",
            StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(txDelay) + "]"));

        nearRtRic->SetAttribute("DefaultLogicModule", PointerValue(defaultLm));
        nearRtRic->SetAttribute("E2Terminator", PointerValue(nearRtRicE2Terminator));
        nearRtRic->SetAttribute("DataRepository", PointerValue(dataRepository));
        nearRtRic->SetAttribute("LmQueryInterval", TimeValue(Seconds(lmQueryInterval)));
        nearRtRic->SetAttribute("ConflictMitigationModule", PointerValue(cmm));

        Simulator::Schedule(Seconds(1), &OranNearRtRic::Start, nearRtRic);

        for (uint32_t idx = 0; idx < ueNodes.GetN(); idx++)
        {
            Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
            Ptr<OranReporterLteUeCellInfo> lteUeCellInfoReporter =
                CreateObject<OranReporterLteUeCellInfo>();
            Ptr<OranReporterAppLoss> appLossReporter = CreateObject<OranReporterAppLoss>();
            Ptr<OranE2NodeTerminatorLteUe> lteUeTerminator =
                CreateObject<OranE2NodeTerminatorLteUe>();

            locationReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));

            lteUeCellInfoReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));

            appLossReporter->SetAttribute("Terminator", PointerValue(lteUeTerminator));
            remoteApps.Get(idx)->TraceConnectWithoutContext(
                "Tx",
                MakeCallback(&ns3::OranReporterAppLoss::AddTx, appLossReporter));
            ueApps.Get(idx)->TraceConnectWithoutContext(
                "Rx",
                MakeCallback(&ns3::OranReporterAppLoss::AddRx, appLossReporter));

            lteUeTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
            lteUeTerminator->SetAttribute("RegistrationIntervalRv",
                                          StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            lteUeTerminator->SetAttribute("SendIntervalRv",
                                          StringValue("ns3::ConstantRandomVariable[Constant=1]"));

            lteUeTerminator->AddReporter(locationReporter);
            lteUeTerminator->AddReporter(lteUeCellInfoReporter);
            lteUeTerminator->AddReporter(appLossReporter);
            lteUeTerminator->SetAttribute("TransmissionDelayRv",
                                          StringValue("ns3::ConstantRandomVariable[Constant=" +
                                                      std::to_string(txDelay) + "]"));

            lteUeTerminator->Attach(ueNodes.Get(idx));

            Simulator::Schedule(Seconds(1), &OranE2NodeTerminatorLteUe::Activate, lteUeTerminator);
        }

        for (uint32_t idx = 0; idx < enbNodes.GetN(); idx++)
        {
            Ptr<OranReporterLocation> locationReporter = CreateObject<OranReporterLocation>();
            Ptr<OranE2NodeTerminatorLteEnb> lteEnbTerminator =
                CreateObject<OranE2NodeTerminatorLteEnb>();

            locationReporter->SetAttribute("Terminator", PointerValue(lteEnbTerminator));

            lteEnbTerminator->SetAttribute("NearRtRic", PointerValue(nearRtRic));
            lteEnbTerminator->SetAttribute("RegistrationIntervalRv",
                                           StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            lteEnbTerminator->SetAttribute("SendIntervalRv",
                                           StringValue("ns3::ConstantRandomVariable[Constant=1]"));

            lteEnbTerminator->AddReporter(locationReporter);
            lteEnbTerminator->Attach(enbNodes.Get(idx));
            lteEnbTerminator->SetAttribute("TransmissionDelayRv",
                                           StringValue("ns3::ConstantRandomVariable[Constant=" +
                                                       std::to_string(txDelay) + "]"));
            Simulator::Schedule(Seconds(1),
                                &OranE2NodeTerminatorLteEnb::Activate,
                                lteEnbTerminator);
        }
    }
    // ORAN END

    // Erase the trace files if they exist
    std::ofstream trafficOutFile(s_trafficTraceFile, std::ios_base::trunc);
    trafficOutFile.close();
    std::ofstream posOutFile(s_positionTraceFile, std::ios_base::trunc);
    posOutFile.close();
    std::ofstream hoOutFile(s_handoverTraceFile, std::ios_base::trunc);
    hoOutFile.close();

    // Start tracing node locations
    Simulator::Schedule(Seconds(1), &TracePositions, ueNodes);

    // Connect to handover trace so we know when a handover is successfully performed
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOkEnb));

    // Colocar na main antes do Simulator::Run()
	user_ip.resize(numUEs);
	user_delay.assign(numUEs, 0);
	user_jitter.assign(numUEs, 0);
	user_throughput.assign(numUEs, 0);
	user_pdr.assign(numUEs, 100);

	Ptr<FlowMonitor> flowMonitor;
	FlowMonitorHelper flowHelper;

	// mudar remoteHost pro nó servidor
	flowHelper.Install(remoteHost);

	// mudar ueNodes pro NodeContainer que contém os UEs
	flowMonitor = flowHelper.Install(ueNodes);

	std::ofstream qos_vs_time;
	qos_vs_time.open("qos-vs-time.txt", std::ofstream::out | std::ofstream::trunc);
	qos_vs_time << "Time,UE,Delay,Jitter,Throughtput,PDR" << std::endl;
	Simulator::Schedule(management_interval, ThroughputMonitor, &flowHelper, flowMonitor);

	// populate user ip map
	for (uint32_t i = 0; i < ueNodes.GetN(); i++)
	{
		Ptr<Ipv4> remoteIpv4 = ueNodes.Get(i)->GetObject<Ipv4>();
		Ipv4Address remoteIpAddr = remoteIpv4->GetAddress(1, 0).GetLocal();
		user_ip[i] = remoteIpAddr;
	}


    // Tell the simulator how long to run
    Simulator::Stop(simTime + Seconds(20));
    // Run the simulation
    Simulator::Run();
    // Clean up used resources
    Simulator::Destroy();

    return 0;
}
