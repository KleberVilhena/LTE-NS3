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

#include "oran-lm-lte-2-lte-torch-handover.h"

#include "oran-command-lte-2-lte-handover.h"

#include <ns3/abort.h>
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/string.h>
#include <ns3/uinteger.h>

#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OranLmLte2LteTorchHandover");
NS_OBJECT_ENSURE_REGISTERED(OranLmLte2LteTorchHandover);

TypeId
OranLmLte2LteTorchHandover::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::OranLmLte2LteTorchHandover")
            .SetParent<OranLm>()
            .AddConstructor<OranLmLte2LteTorchHandover>()
            .AddAttribute("TorchModelPath",
                          "The file path of the ML model.",
                          StringValue("saved_trained_classification_pytorch.pt"),
                          MakeStringAccessor(&OranLmLte2LteTorchHandover::SetTorchModelPath),
                          MakeStringChecker());

    return tid;
}

OranLmLte2LteTorchHandover::OranLmLte2LteTorchHandover(void)
{
    NS_LOG_FUNCTION(this);

    m_name = "OranLmLte2LteTorchHandover";
}

OranLmLte2LteTorchHandover::~OranLmLte2LteTorchHandover(void)
{
    NS_LOG_FUNCTION(this);
}

std::vector<Ptr<OranCommand>>
OranLmLte2LteTorchHandover::Run(void)
{
    NS_LOG_FUNCTION(this);

    std::vector<Ptr<OranCommand>> commands;

    if (m_active)
    {
        NS_ABORT_MSG_IF(m_nearRtRic == nullptr,
                        "Attempting to run LM (" + m_name + ") with NULL Near-RT RIC");

        Ptr<OranDataRepository> data = m_nearRtRic->Data();
        std::vector<UeInfo> ueInfos = GetUeInfos(data);
        std::vector<EnbInfo> enbInfos = GetEnbInfos(data);
        commands = GetHandoverCommands(data, ueInfos, enbInfos);
    }

    return commands;
}

void
OranLmLte2LteTorchHandover::SetTorchModelPath(const std::string& torchModelPath)
{
    std::ifstream f(torchModelPath.c_str());
    NS_ABORT_MSG_IF(!f.good(),
                    "Torch model file \""
                        << torchModelPath << "\" not found."
                        << " Sample model \"saved_trained_classification_pytorch.pt\""
                        << " can be copied from the example folder to the working directory.");
    f.close();

    try
    {
        m_model = torch::jit::load(torchModelPath);
    }
    catch (const c10::Error& e)
    {
        NS_ABORT_MSG("Could not load trained ML model.");
    }
}

std::vector<OranLmLte2LteTorchHandover::UeInfo>
OranLmLte2LteTorchHandover::GetUeInfos(Ptr<OranDataRepository> data) const
{
    NS_LOG_FUNCTION(this << data);

    std::vector<UeInfo> ueInfos;
    for (const auto ueId : data->GetLteUeE2NodeIds())
    {
        UeInfo ueInfo;
        ueInfo.nodeId = ueId;
        bool found;
        std::tie(found, ueInfo.cellId, ueInfo.rnti) = data->GetLteUeCellInfo(ueInfo.nodeId);
        if (found)
        {
            std::map<Time, Vector> nodePositions =
                data->GetNodePositions(ueInfo.nodeId, Seconds(0), Simulator::Now());
            if (!nodePositions.empty())
            {
                ueInfo.position = nodePositions.rbegin()->second;
                ueInfo.loss = data->GetAppLoss(ueInfo.nodeId);
                ueInfos.push_back(ueInfo);
            }
            else
            {
                NS_LOG_INFO("Could not find LTE UE location for E2 Node ID = " << ueInfo.nodeId);
            }
        }
        else
        {
            NS_LOG_INFO("Could not find LTE UE cell info for E2 Node ID = " << ueInfo.nodeId);
        }
    }
    return ueInfos;
}

std::vector<OranLmLte2LteTorchHandover::EnbInfo>
OranLmLte2LteTorchHandover::GetEnbInfos(Ptr<OranDataRepository> data) const
{
    NS_LOG_FUNCTION(this << data);

    std::vector<EnbInfo> enbInfos;
    for (const auto enbId : data->GetLteEnbE2NodeIds())
    {
        EnbInfo enbInfo;
        enbInfo.nodeId = enbId;
        bool found;
        std::tie(found, enbInfo.cellId) = data->GetLteEnbCellInfo(enbInfo.nodeId);
        if (found)
        {
            std::map<Time, Vector> nodePositions =
                data->GetNodePositions(enbInfo.nodeId, Seconds(0), Simulator::Now());

            if (!nodePositions.empty())
            {
                enbInfo.position = nodePositions.rbegin()->second;
                enbInfos.push_back(enbInfo);
            }
            else
            {
                NS_LOG_INFO("Could not find LTE eNB location for E2 Node ID = " << enbInfo.nodeId);
            }
        }
        else
        {
            NS_LOG_INFO("Could not find LTE eNB cell info for E2 Node ID = " << enbInfo.nodeId);
        }
    }
    return enbInfos;
}

std::vector<Ptr<OranCommand>>
OranLmLte2LteTorchHandover::GetHandoverCommands(
    Ptr<OranDataRepository> data,
    std::vector<OranLmLte2LteTorchHandover::UeInfo> ueInfos,
    std::vector<OranLmLte2LteTorchHandover::EnbInfo> enbInfos)
{
    NS_LOG_FUNCTION(this << data);

    std::vector<Ptr<OranCommand>> commands;

	//key: uenodeid
    std::map<uint16_t, std::vector<std::pair<OranLmLte2LteTorchHandover::EnbInfo, float>>> distanceEnb;
	//key: uenodeid
    std::map<uint16_t, float> loss;
	//key: cellid
    std::map<uint16_t, int> ueCount;
	//key: cellid
    std::map<uint16_t, float> meanLossEnb;
	int numUEs = ueInfos.size();

    for (const auto ueInfo : ueInfos)
    {
		std::vector<std::pair<OranLmLte2LteTorchHandover::EnbInfo, float>> dists;
        for (const auto enbInfo : enbInfos)
        {
            float d = std::sqrt(std::pow(ueInfo.position.x - enbInfo.position.x, 2) +
                                std::pow(ueInfo.position.y - enbInfo.position.y, 2));
			auto item = std::make_pair(enbInfo,d);
			dists.push_back(item);
        }
		std::stable_sort(dists.begin(),dists.end(), [&]
					(std::pair<OranLmLte2LteTorchHandover::EnbInfo,float> a,
					 std::pair<OranLmLte2LteTorchHandover::EnbInfo,float> b)
					{ return (a.second<b.second); });
		dists.resize(3);
		for(int i = 0; i < 3; ++i)
			dists[i].second /= dists[2].second;
		distanceEnb[ueInfo.nodeId] = dists;
        loss[ueInfo.nodeId] = ueInfo.loss;
		ueCount[ueInfo.cellId]++;
		meanLossEnb[ueInfo.cellId] += ueInfo.loss;
    }

	for (const auto enbInfo : enbInfos)
		meanLossEnb[enbInfo.cellId] /= ueCount[enbInfo.cellId];

    for (const auto ueInfo : ueInfos)
    {
		auto enb_data = distanceEnb[ueInfo.nodeId];
		std::vector<float> inputv = {enb_data[0].second,
									 enb_data[1].second,
									 enb_data[2].second,
									 meanLossEnb[enb_data[0].first.cellId],
									 meanLossEnb[enb_data[1].first.cellId],
									 meanLossEnb[enb_data[2].first.cellId],
									 loss[ueInfo.nodeId]};

		LogLogicToRepository("ML input tensor: (" + std::to_string(inputv.at(0)) + ", " +
							 std::to_string(inputv.at(1)) + ", " + std::to_string(inputv.at(2)) + ", " +
							 std::to_string(inputv.at(3)) + ", " + std::to_string(inputv.at(4)) + ", " +
							 std::to_string(inputv.at(5)) + ", " + std::to_string(inputv.at(6)) + ", " + ")");

		std::vector<torch::jit::IValue> inputs;
		inputs.push_back(torch::from_blob(inputv.data(), {1, 7}).to(torch::kFloat32));
		at::Tensor output = torch::softmax(m_model.forward(inputs).toTensor(), 1);

		int cellId = enb_data[output.argmax(1).item().toInt()].first.cellId;
		LogLogicToRepository("ML Chooses cell " + std::to_string(cellId));

		if (cellId == ueInfo.cellId)
			continue;

		Ptr<OranCommandLte2LteHandover> handoverCommand =
			CreateObject<OranCommandLte2LteHandover>();
		handoverCommand->SetAttribute("TargetE2NodeId", UintegerValue(numUEs+ueInfo.cellId));
		handoverCommand->SetAttribute("TargetRnti", UintegerValue(ueInfo.rnti));
		handoverCommand->SetAttribute("TargetCellId", UintegerValue(cellId));
		data->LogCommandLm(m_name, handoverCommand);
		commands.push_back(handoverCommand);

		LogLogicToRepository("Moving UE " + std::to_string(ueInfo.nodeId) +
							" from Cell ID " + std::to_string(ueInfo.cellId) +
							" to Cell ID " + std::to_string(cellId));
    }

    return commands;
}

} // namespace ns3
