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

#include <math.h>

using namespace ns3;

static std::string s_trafficTraceFile = "traffic-trace.tr";
static std::string s_positionTraceFile = "position-trace.tr";
static std::string s_handoverTraceFile = "handover-trace.tr";
static std::string ns3_dir;

// Function that will save the traces of RX'd packets
void
RxTrace(Ptr<const Packet> p, const Address& from, const Address& to)
{
    uint16_t ueId = (InetSocketAddress::ConvertFrom(to).GetPort() / 1000);

    std::ofstream rxOutFile(s_trafficTraceFile, std::ios_base::app);
    rxOutFile << Simulator::Now().GetSeconds() << " " << ueId << " RX " << p->GetSize()
              << std::endl;
}

void
RXCallBack(Ptr<OranReporterAppLoss> reporter, Ptr<const Packet> p, const Address& from, const Address& to)
{
	reporter->AddRx(p, from);
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

NS_LOG_COMPONENT_DEFINE("OranLte2LteMlHandoverTrain");

std::vector<Vector> load_position_trace() {
    std::ifstream infile(ns3_dir + "train-initial-positions.tr");
    std::vector<Vector> pos;
    double x, y;
    while (infile >> x >> y) {
        Vector tmp = {x, y, 0};
        pos.push_back(tmp);
    }
    infile.close();
    return pos;
}

bool IsTopLevelSourceDir (std::string path)
{
	bool haveVersion = false;
	bool haveLicense = false;

	//
	// If there's a file named VERSION and a file named LICENSE in this
	// directory, we assume it's our top level source directory.
	//

	std::list<std::string> files = SystemPath::ReadFiles (path);
	for (std::list<std::string>::const_iterator i = files.begin (); i != files.end (); ++i)
	{
		if (*i == "VERSION")
		{
			haveVersion = true;
		}
		else if (*i == "LICENSE")
		{
			haveLicense = true;
		}
	}

	return haveVersion && haveLicense;
}

std::string GetTopLevelSourceDir (void)
{
	std::string self = SystemPath::FindSelfDirectory ();
	std::list<std::string> elements = SystemPath::Split (self);
	while (!elements.empty ())
	{
		std::string path = SystemPath::Join (elements.begin (), elements.end ());
		if (IsTopLevelSourceDir (path))
		{
			return path + "/";
		}
		elements.pop_back ();
	}
	NS_FATAL_ERROR ("Could not find source directory from self=" << self);
	return "";
}

int
main(int argc, char* argv[])
{
    bool verbose = false;
    bool useOran = true;
    bool useOnnx = false;
    bool useTorch = false;
    bool useDistance = false;
    uint32_t startConfig = 0;
    double lmQueryInterval = 1;
    double txDelay = 0;
	int numUEs=4;
	int scenario=0;
	int runId=0;
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
    cmd.AddValue("scenario", "The simulation scenario", scenario);
    cmd.AddValue("start-config", "The starting configuration", startConfig);
    cmd.AddValue("run-id", "The run id. It changes the first UE start position", runId);
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
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(useOran == false && (useOnnx || useTorch || useDistance),
                    "Cannot use ML LM or distance LM without enabling O-RAN.");
    NS_ABORT_MSG_IF((useOnnx + useTorch + useDistance) > 1,
                    "Cannot use more than one LM simultaneously.");
    NS_ABORT_MSG_IF(handoverAlgorithm != "ns3::NoOpHandoverAlgorithm" &&
                        (useOnnx || useTorch || useDistance),
                    "Cannot use non-noop handover algorithm with ML LM or distance LM.");

	ns3_dir = GetTopLevelSourceDir();

    // Increase the buffer size to accomodate the application demand
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(100 * 1024));
    // Disabled to prevent the automatic cell reselection when signal quality is bad.
    Config::SetDefault("ns3::LteUePhy::EnableRlfDetection", BooleanValue(false));
	Config::SetDefault("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(true));
    Config::SetDefault("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UsePdschForCqiGeneration", BooleanValue(true));

    // Uplink Power Control
    Config::SetDefault("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue(true));
    Config::SetDefault("ns3::LteUePowerControl::ClosedLoop", BooleanValue(true));
    Config::SetDefault("ns3::LteUePowerControl::AccumulationEnabled", BooleanValue(false));

    // Configure the LTE parameters (pathloss, bandwidth, scheduler)
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::Cost231PropagationLossModel"));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue (43));
	lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(50));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(50));
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");
    lteHelper->SetSchedulerAttribute("HarqEnabled", BooleanValue(true));
    lteHelper->SetHandoverAlgorithmType(handoverAlgorithm);

    // Deploy the EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

	lteHelper->SetFfrAlgorithmType("ns3::LteFrHardAlgorithm");

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

    if (scenario >= 3)
    {
        NS_ABORT_MSG("Scenario " << scenario << " not supported.");
    }

	switch(scenario) {
		case 0:
			numUEs = 4;
			break;
		case 1:
			numUEs = 16;
			break;
		case 2:
			numUEs = 25;
			break;
	}

    // Create eNB and UE
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(3);
    ueNodes.Create(numUEs);

    // Install Mobility Model for eNB (Constant Position at (0, 0, 0)
    Ptr<ListPositionAllocator> positionAllocEnbs = CreateObject<ListPositionAllocator>();
    positionAllocEnbs->Add(Vector(-250, 145, 30));
    positionAllocEnbs->Add(Vector(250, 145, 30));
    positionAllocEnbs->Add(Vector(0, -288, 30));
    MobilityHelper mobilityEnbs;
    mobilityEnbs.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnbs.SetPositionAllocator(positionAllocEnbs);
    mobilityEnbs.Install(enbNodes);

    // Install Mobility Model for UE (Constant Positions)
	auto pos = load_position_trace();
    Ptr<ListPositionAllocator> positionUe = CreateObject<ListPositionAllocator>();
	positionUe->Add(pos[runId]);

    Ptr<ListPositionAllocator> positionAllocUes = CreateObject<ListPositionAllocator>();
	//config 0 + 1
    positionAllocUes->Add(Vector(-231, 116, 0));
    positionAllocUes->Add(Vector(222, 149, 0));
    positionAllocUes->Add(Vector(28, -273, 0));
	//config 1
    positionAllocUes->Add(Vector(233, 104, 0));
    positionAllocUes->Add(Vector(284, 129, 0));
    positionAllocUes->Add(Vector(267, 110, 0));

    positionAllocUes->Add(Vector(-25, -256, 0));
    positionAllocUes->Add(Vector(-57, -282, 0));
    positionAllocUes->Add(Vector(-29, -316, 0));
    positionAllocUes->Add(Vector(40, -311, 0));
    positionAllocUes->Add(Vector(13, -243, 0));
    positionAllocUes->Add(Vector(-8, -341, 0));
    positionAllocUes->Add(Vector(-57, -333, 0));
    positionAllocUes->Add(Vector(26, -334, 0));
    positionAllocUes->Add(Vector(0, -316, 0));
    MobilityHelper mobilityUes;

    // Mobility Model for UEs
    Ptr<RandomVariableStream> speedRvs =
        CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                          DoubleValue(1),
                                                          "Max",
                                                          DoubleValue(2.5));
    Ptr<RandomVariableStream> pauseRvs =
        CreateObjectWithAttributes<UniformRandomVariable>("Min",
                                                          DoubleValue(1),
                                                          "Max",
                                                          DoubleValue(6));
    mobilityUes.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                                 "Bounds",
                                 RectangleValue(Rectangle(-800, 800, -630, 630)),
                                 "Speed",
                                 PointerValue(speedRvs),
                                 "Pause",
                                 PointerValue(pauseRvs));
    mobilityUes.SetPositionAllocator(positionUe);
	mobilityUes.Install(ueNodes.Get(0));

	mobilityUes.SetPositionAllocator(positionAllocUes);
	for(int i = 1; i<4; ++i)
		mobilityUes.Install(ueNodes.Get(i));
	if (scenario == 2) {
		mobilityUes.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
						"X", StringValue ("ns3::UniformRandomVariable[Min=-790.0|Max=790.0]"),
						"Y", StringValue ("ns3::UniformRandomVariable[Min=-620.0|Max=620.0]"));
	}
	for(int i = 4; i<numUEs; ++i)
		mobilityUes.Install(ueNodes.Get(i));

	// Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs;
		for (uint32_t i = 0; i < 3; ++i) {
		lteHelper->SetFfrAlgorithmAttribute(
				"FrCellTypeId", UintegerValue(i+1));
		enbLteDevs.Add(lteHelper->InstallEnbDevice(enbNodes.Get(i)));
	}
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

    if (startConfig >= 3)
    {
        NS_ABORT_MSG("Starting configuration " << startConfig << " not supported.");
    }
	lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(startConfig%3));

	for(int i = 1; i<numUEs; ++i)
		lteHelper->AttachToClosestEnb(ueLteDevs.Get(i), enbLteDevs);

    lteHelper->AddX2Interface(enbNodes);

    // Install and start applications on UEs and remote host
    uint16_t basePort = 1000;
    ApplicationContainer remoteApps;
    ApplicationContainer ueApps;

    for (uint16_t i = 0; i < ueNodes.GetN(); i++)
    {
        uint16_t port = basePort * (i + 1);
		DataRate dataRate("3Mbps");
		uint64_t bitRate = dataRate.GetBitRate();
		uint32_t packetSize = 1400; // bytes
		double interPacketInterval = static_cast<double>(packetSize * 8) / bitRate;
		Time udpInterval = Seconds(interPacketInterval);

		UdpServerHelper server(port);
        ueApps.Add(server.Install(ueNodes.Get(i)));
        // Enable the tracing of RX packets
        ueApps.Get(i)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxTrace));

		UdpClientHelper client(ueIpIface.GetAddress(i), port);
        // Attributes
		client.SetAttribute("Interval", TimeValue(udpInterval));
		client.SetAttribute("PacketSize", UintegerValue(packetSize));
		client.SetAttribute("MaxPackets", UintegerValue(0)); //unlimited
        remoteApps.Add(client.Install(remoteHost));
		// Enable the tracing of TX packets
        remoteApps.Get(i)->TraceConnectWithoutContext("TxWithAddresses", MakeCallback(&TxTrace));
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
                "RxWithAddresses",
                MakeBoundCallback(&RXCallBack, appLossReporter));

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

    // Tell the simulator how long to run
    Simulator::Stop(simTime + Seconds(20));
    // Run the simulation
    Simulator::Run();
    // Clean up used resources
    Simulator::Destroy();

    return 0;
}
