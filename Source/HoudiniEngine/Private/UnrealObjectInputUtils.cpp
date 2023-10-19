﻿#include "UnrealObjectInputUtils.h"

#include "UObject/Object.h"
#include "LandscapeSplinesComponent.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputManager.h"
#include "UnrealObjectInputRuntimeUtils.h"


bool
FUnrealObjectInputUtils::FindNodeViaManager(
	const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputHandle Handle;
	if (!Manager->FindNode(InIdentifier, Handle))
		return false;

	OutHandle = Handle;
	return true;
}

FUnrealObjectInputNode*
FUnrealObjectInputUtils::GetNodeViaManager(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid())
		return nullptr;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return nullptr;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InHandle, Node))
		return nullptr;

	return Node;
}

FUnrealObjectInputNode*
FUnrealObjectInputUtils::GetNodeViaManager(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!InIdentifier.IsValid())
		return nullptr;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return nullptr;

	FUnrealObjectInputHandle Handle;
	if (!Manager->FindNode(InIdentifier, Handle))
		return nullptr;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(Handle, Node))
		return nullptr;

	OutHandle = Handle;
	return Node;
}

bool
FUnrealObjectInputUtils::FindParentNodeViaManager(
	const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle)
{
	if (!InIdentifier.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InIdentifier, Node) || !Node)
		return false;
	
	OutParentHandle = Node->GetParent();
	return true;
}

bool
FUnrealObjectInputUtils::AreHAPINodesValid(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid())
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->AreHAPINodesValid(InHandle);
}

bool
FUnrealObjectInputUtils::NodeExistsAndIsNotDirty(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutHandle)
{
	if (!FindNodeViaManager(InIdentifier, OutHandle) || !AreHAPINodesValid(OutHandle) || FUnrealObjectInputRuntimeUtils::IsInputNodeDirty(InIdentifier))
		return false;

	return true;
}

bool
FUnrealObjectInputUtils::AreReferencedHAPINodesValid(const FUnrealObjectInputHandle& InHandle)
{
	if (!InHandle.IsValid() || InHandle.GetIdentifier().GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;
	
	// Check with the manager if there already is a node for this object
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode const* Node = nullptr;
	if (!Manager->GetNode(InHandle, Node))
		return false;

	if (!Node)
		return false;

	FUnrealObjectInputReferenceNode const* const RefNode = static_cast<FUnrealObjectInputReferenceNode const*>(Node);
	return RefNode->AreReferencedHAPINodesValid();
}

bool
FUnrealObjectInputUtils::AddNodeOrUpdateNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const int32 InNodeId,
	FUnrealObjectInputHandle& OutHandle,
	const int32 InObjectNodeId,
	TSet<FUnrealObjectInputHandle> const* const InReferencedNodes,
	const bool& bInputNodesCanBeDeleted,
	const TOptional<int32> InReferencesConnectToNodeId)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager* Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputHandle Handle;
	const bool bNodeExists = FindNodeViaManager(InIdentifier, Handle) && Handle.IsValid();

	bool bSuccess = false;
	switch (InIdentifier.GetNodeType())
	{
		case EUnrealObjectInputNodeType::Container:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateContainer(InIdentifier, InNodeId);
			}
			else
			{
				bSuccess = Manager->AddContainer(InIdentifier, InNodeId, Handle);
			}
			break;
		case EUnrealObjectInputNodeType::Reference:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateReferenceNode(InIdentifier, InObjectNodeId, InNodeId, InReferencedNodes, InReferencesConnectToNodeId);
			}
			else
			{
				bSuccess = Manager->AddReferenceNode(InIdentifier, InObjectNodeId, InNodeId, Handle, InReferencedNodes, InReferencesConnectToNodeId.Get(INDEX_NONE));
			}
			break;
		case EUnrealObjectInputNodeType::Leaf:
			if (bNodeExists)
			{
				bSuccess = Manager->UpdateLeaf(InIdentifier, InObjectNodeId, InNodeId);
			}
			else
			{
				bSuccess = Manager->AddLeaf(InIdentifier, InObjectNodeId, InNodeId, Handle);
			}
			break;
		case EUnrealObjectInputNodeType::Invalid:
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::AddNodeOrUpdateNode] Invalid NodeType in identifier."));
			bSuccess = false;
			break;
	}

	if (!bSuccess)
		return false;

	// Make sure to prevent deletion of the node and its parents if needed
	if (!bInputNodesCanBeDeleted)
	{
		//Manager->EnsureParentsExist(InIdentifier, Handle, bInputNodesCanBeDeleted);
		FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(Handle, bInputNodesCanBeDeleted);
	}

	OutHandle = Handle;
	return bSuccess;
}

bool
FUnrealObjectInputUtils::GetHAPINodeId(const FUnrealObjectInputHandle& InHandle, int32& OutNodeId)
{
	if (!InHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;
	
	const FUnrealObjectInputNode* Node;
	Manager->GetNode(InHandle, Node);
	OutNodeId = Node->GetHAPINodeId();

	return true;
}

bool
FUnrealObjectInputUtils::UpdateInputNodeCanBeDeleted(const FUnrealObjectInputHandle& InHandle, const bool& bCanBeDeleted)
{
	if (!InHandle.IsValid())
		return false;

	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	Manager->GetNode(InHandle, Node);

	if (!Node)
		return false;

	// Only set the value to false if it isnt already
	if(!bCanBeDeleted)
		Node->SetCanBeDeleted(bCanBeDeleted);

	return true;
}

bool
FUnrealObjectInputUtils::GetDefaultInputNodeName(const FUnrealObjectInputIdentifier& InIdentifier, FString& OutNodeName)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	OutNodeName = Manager->GetDefaultNodeName(InIdentifier);
	return true;
}

bool
FUnrealObjectInputUtils::EnsureParentsExist(const FUnrealObjectInputIdentifier& InIdentifier, FUnrealObjectInputHandle& OutParentHandle, const bool& bInputNodesCanBeDeleted)
{
	if (!InIdentifier.IsValid())
		return false;
	
	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->EnsureParentsExist(InIdentifier, OutParentHandle, bInputNodesCanBeDeleted);
}

bool
FUnrealObjectInputUtils::SetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, const TSet<FUnrealObjectInputHandle>& InReferencedNodes)
{
	if (!InRefNodeHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeHandle, Node))
		return false;

	FUnrealObjectInputReferenceNode* RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->SetReferencedNodes(InReferencedNodes);
	return true;
}

bool
FUnrealObjectInputUtils::GetReferencedNodes(const FUnrealObjectInputHandle& InRefNodeHandle, TSet<FUnrealObjectInputHandle>& OutReferencedNodes)
{
	if (!InRefNodeHandle.IsValid())
		return false;
	
	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeHandle, Node))
		return false;

	FUnrealObjectInputReferenceNode* RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->GetReferencedNodes(OutReferencedNodes);
	return true;
}

bool
FUnrealObjectInputUtils::BuildMeshInputObjectIdentifiers(
	UObject const* const InInputObject,
	const bool bInExportMainMesh,
	const bool bInExportLODs,
	const bool bInExportSockets,
	const bool bInExportColliders,
	bool &bOutSingleLeafNodeOnly,
	FUnrealObjectInputIdentifier& OutReferenceNode,
	TArray<FUnrealObjectInputIdentifier>& OutPerOptionIdentifiers)
{
	constexpr bool bDefaultImportAsReference = false;
	constexpr bool bDefaultImportAsReferenceRotScaleEnabled = false;
	constexpr bool bDefaultExportLODs = false;
	constexpr bool bDefaultExportSockets = false;
	constexpr bool bDefaultExportColliders = false;
	const FUnrealObjectInputOptions DefaultOptions(
		bDefaultImportAsReference,
		bDefaultImportAsReferenceRotScaleEnabled,
		bDefaultExportLODs,
		bDefaultExportSockets,
		bDefaultExportColliders);

	// Determine number of leaves
	uint32 NumLeaves = 0;
	if (bInExportMainMesh)
		NumLeaves++;
	if (bInExportLODs)
		NumLeaves++;
	if (bInExportSockets)
		NumLeaves++;
	if (bInExportColliders)
		NumLeaves++;
	if (NumLeaves <= 1)
	{
		// Only one active option, we can create a leaf node and not a reference node
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = bInExportLODs;
		Options.bExportSockets = bInExportSockets;
		Options.bExportColliders = bInExportColliders;

		constexpr bool bIsLeaf = true;
		bOutSingleLeafNodeOnly = true;
		OutReferenceNode = FUnrealObjectInputIdentifier();
		OutPerOptionIdentifiers = {FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf)};
		return true;
	}

	bOutSingleLeafNodeOnly = false;
	// Construct the reference node's identifier
	{
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = bInExportLODs;
		Options.bExportSockets = bInExportSockets;
		Options.bExportColliders = bInExportColliders;
		
		constexpr bool bIsLeaf = false;
		OutReferenceNode = FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf);
	}

	// Construct per-option identifiers
	TArray<FUnrealObjectInputIdentifier> PerOptionIdentifiers;
	// First one is the main mesh only, so all options false
	if (bInExportMainMesh)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		// TODO: add a specific main mesh option?
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}
	
	if (bInExportLODs)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportLODs = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	if (bInExportSockets)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportSockets = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	if (bInExportColliders)
	{
		constexpr bool bIsLeaf = true;
		FUnrealObjectInputOptions Options = DefaultOptions;
		Options.bExportColliders = true;
		PerOptionIdentifiers.Add(FUnrealObjectInputIdentifier(InInputObject, Options, bIsLeaf));
	}

	OutPerOptionIdentifiers = MoveTemp(PerOptionIdentifiers);
	return true;
}

bool
FUnrealObjectInputUtils::SetObjectMergeXFormTypeToWorldOrigin(const HAPI_NodeId& InObjMergeNodeId)
{
	HAPI_Session const* const Session = FHoudiniEngine::Get().GetSession();
	
	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;
		
	const HAPI_NodeId WorldOriginNodeId = Manager->GetWorldOriginHAPINodeId();
	if (WorldOriginNodeId < 0)
	{
		HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::SetObjectMergeXFormTypeToWorldOrigin] Could not find/create world origin null."));
		return false;
	}

	// Set the transform value to "Into Specified Object"
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmIntValue(Session, InObjMergeNodeId, TCHAR_TO_UTF8(TEXT("xformtype")), 0, 2), false);
	// Set the transform object to the world origin null from the manager
	HOUDINI_CHECK_ERROR_RETURN(
		FHoudiniApi::SetParmNodeValue(Session, InObjMergeNodeId, TCHAR_TO_UTF8(TEXT("xformpath")), WorldOriginNodeId), false);

	return true;
}

bool
FUnrealObjectInputUtils::SetReferencesNodeConnectToNodeId(const FUnrealObjectInputIdentifier& InRefNodeIdentifier, const HAPI_NodeId InNodeId)
{
	// Identifier must be valid and for a reference node
	if (!InRefNodeIdentifier.IsValid() || InRefNodeIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;

	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeIdentifier, Node))
		return false;

	FUnrealObjectInputReferenceNode* const RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	RefNode->SetReferencesConnectToNodeId(InNodeId);

	return true;
}

bool
FUnrealObjectInputUtils::ConnectReferencedNodesToMerge(const FUnrealObjectInputIdentifier& InRefNodeIdentifier)
{
	// Identifier must be valid and for a reference node
	if (!InRefNodeIdentifier.IsValid() || InRefNodeIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;

	if (!AreHAPINodesValid(InRefNodeIdentifier))
		return false;
	
	if (!AreReferencedHAPINodesValid(InRefNodeIdentifier))
		return false;

	IUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	FUnrealObjectInputNode* Node = nullptr;
	if (!Manager->GetNode(InRefNodeIdentifier, Node))
		return false;

	FUnrealObjectInputReferenceNode* const RefNode = static_cast<FUnrealObjectInputReferenceNode*>(Node);
	if (!RefNode)
		return false;

	const HAPI_Session* const Session = FHoudiniEngine::Get().GetSession();
	const FUnrealObjectInputHAPINodeId RefNodeId = RefNode->GetReferencesConnectToNodeId();
	if (!RefNodeId.IsValid())
		return false;

	const HAPI_NodeId RefHAPINodeId = RefNodeId.GetHAPINodeId();
	int32 InputIndex = 0;
	for (const FUnrealObjectInputHandle& Handle : RefNode->GetReferencedNodes())
	{
		HAPI_NodeId NodeId = -1;
		if (!GetHAPINodeId(Handle, NodeId))
			continue;

		// Connect the current input object to the merge node
		if (FHoudiniApi::ConnectNodeInput(Session, RefHAPINodeId, InputIndex, NodeId, 0) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;
		}
		// HAPI will automatically create a object_merge node, we need to set the xformtype to "Into specified object"
		HAPI_NodeId ConnectedNodeId = -1;
		if (FHoudiniApi::QueryNodeInput(Session, RefHAPINodeId, InputIndex, &ConnectedNodeId) != HAPI_RESULT_SUCCESS)
		{
			HOUDINI_LOG_WARNING(TEXT("[FUnrealObjectInputUtils::ConnectReferencedNodes] Failed to query connected node input: %s"), *FHoudiniEngineUtils::GetErrorDescription());
			continue;

		}
		InputIndex++;

		if (ConnectedNodeId < 0)
		{
			// No connected was made even though previous functions were successful!?
			continue;
		}

		// Set the transform value to "Into Specified Object"
		// Set the transform object to the world origin null from the manager
		SetObjectMergeXFormTypeToWorldOrigin(ConnectedNodeId);
	}

	// Disconnect input indices >= InputIndex
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	if (FHoudiniApi::GetNodeInfo(Session, RefHAPINodeId, &NodeInfo) == HAPI_RESULT_SUCCESS)
	{
		while (InputIndex < NodeInfo.inputCount)
		{
			// There is currently no way to get the number of _connected_ input nodes or a way to compose a list of the
			// connected input nodes. So we query the node input at each index until we find an index without a
			// connection.
			HAPI_NodeId ConnectedInputNodeId = -1;
			if (FHoudiniApi::QueryNodeInput(Session, RefHAPINodeId, InputIndex, &ConnectedInputNodeId) != HAPI_RESULT_SUCCESS || ConnectedInputNodeId < 0)
				break;

			HOUDINI_CHECK_ERROR(FHoudiniApi::DisconnectNodeInput(Session, RefHAPINodeId, InputIndex));

			InputIndex++;
		}
	}

	return true;
}

bool
FUnrealObjectInputUtils::CreateOrUpdateReferenceInputMergeNode(
	const FUnrealObjectInputIdentifier& InIdentifier,
	const TSet<FUnrealObjectInputHandle>& InReferencedNodes,
	FUnrealObjectInputHandle& OutHandle,
	const bool bInConnectReferencedNodes,
	const bool& bInputNodesCanBeDeleted)
{
	// Identifier must be valid and for a reference node
	if (!InIdentifier.IsValid() || InIdentifier.GetNodeType() != EUnrealObjectInputNodeType::Reference)
		return false;
	
	FUnrealObjectInputHandle Handle;
	if (!FindNodeViaManager(InIdentifier, Handle) || !AreHAPINodesValid(Handle))
	{
		// Add the node without an node id first
		// Then get its parent id and create the HAPI node inside the parent's network
		int32 ObjectNodeId = -1;
		int32 NodeId = -1;		
		if (!AddNodeOrUpdateNode(InIdentifier, NodeId, Handle, ObjectNodeId, &InReferencedNodes, bInputNodesCanBeDeleted))
			return false;

		// Get the parent node id
		int32 ParentNodeId = -1;
		FUnrealObjectInputHandle ParentHandle;
		if (EnsureParentsExist(InIdentifier, ParentHandle, bInputNodesCanBeDeleted))
			GetHAPINodeId(ParentHandle, ParentNodeId);

		FString NodeName;
		GetDefaultInputNodeName(InIdentifier, NodeName);
		// Create the object node
		if (FHoudiniEngineUtils::CreateNode(ParentNodeId, TEXT("geo"), NodeName, true, &ObjectNodeId) != HAPI_RESULT_SUCCESS)
			return false;
		// Create the merge node
		if (FHoudiniEngineUtils::CreateNode(ObjectNodeId, TEXT("merge"), NodeName, true, &NodeId) != HAPI_RESULT_SUCCESS)
			return false;
		
		if (!AddNodeOrUpdateNode(InIdentifier, NodeId, Handle, ObjectNodeId, &InReferencedNodes, bInputNodesCanBeDeleted))
			return false;

		if (bInConnectReferencedNodes)
		{
			if (!ConnectReferencedNodesToMerge(InIdentifier))
				return false;
		}

		FUnrealObjectInputRuntimeUtils::ClearInputNodeDirtyFlag(InIdentifier);
		
		OutHandle = Handle;
		return true;
	}

	SetReferencedNodes(Handle, InReferencedNodes);
	if (bInConnectReferencedNodes)
	{
		if (!ConnectReferencedNodesToMerge(InIdentifier))
			return false;
	}

	FUnrealObjectInputRuntimeUtils::ClearInputNodeDirtyFlag(InIdentifier);

	OutHandle = Handle;
	return true;
}

bool
FUnrealObjectInputUtils::AddModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, const int32 InNodeIdToConnectTo)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;
	
	return Node->AddModifierChain(InChainName, InNodeIdToConnectTo);
}

bool
FUnrealObjectInputUtils::SetModifierChainNodeToConnectTo(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, const int32 InNodeToConnectTo)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;
	
	return Node->SetModifierChainNodeToConnectTo(InChainName, InNodeToConnectTo);
}

bool
FUnrealObjectInputUtils::DoesModifierChainExist(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->GetModifierChain(InChainName) != nullptr; 
}

HAPI_NodeId
FUnrealObjectInputUtils::GetInputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return INDEX_NONE;

	return Node->GetInputHAPINodeIdOfModifierChain(InChainName);
}

HAPI_NodeId
FUnrealObjectInputUtils::GetOutputNodeOfModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return INDEX_NONE;

	return Node->GetOutputHAPINodeIdOfModifierChain(InChainName);
}

bool
FUnrealObjectInputUtils::RemoveModifierChain(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->RemoveModifierChain(InChainName);
}

bool
FUnrealObjectInputUtils::DestroyModifier(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName, FUnrealObjectInputModifier* const InModifierToDestroy)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->DestroyModifier(InChainName, InModifierToDestroy);
}

FUnrealObjectInputModifier*
FUnrealObjectInputUtils::FindFirstModifierOfType(const FUnrealObjectInputIdentifier& InIdentifier, FName InChainName, EUnrealObjectInputModifierType InModifierType)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode const* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return nullptr;

	return Node->FindFirstModifierOfType(InChainName, InModifierType);
}

bool
FUnrealObjectInputUtils::UpdateModifiers(const FUnrealObjectInputIdentifier& InIdentifier, const FName InChainName)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->UpdateModifiers(InChainName);
}

bool
FUnrealObjectInputUtils::UpdateAllModifierChains(const FUnrealObjectInputIdentifier& InIdentifier)
{
	FUnrealObjectInputHandle Handle;
	FUnrealObjectInputNode* const Node = GetNodeViaManager(InIdentifier, Handle);
	if (!Node || !Handle.IsValid())
		return false;

	return Node->UpdateAllModifierChains();
}

