﻿#pragma once


// Houdini Engine forward declarations
class FUnrealObjectInputIdentifier;


struct HOUDINIENGINERUNTIME_API FUnrealObjectInputRuntimeUtils
{
	public:
		// Returns true if the reference counted input system is enabled
		static bool IsRefCountedInputSystemEnabled();

		// Helper for checking if an input node is marked as dirty in the ref counted input system.
		static bool IsInputNodeDirty(const FUnrealObjectInputIdentifier& InIdentifier);

		// Helper for marking an input node, and optionally for reference nodes its referenced nodes, as dirty in the ref counted input system
		static bool MarkInputNodeAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, bool bInAlsoDirtyReferencedNodes);

		// Helper for clearing the dirty flag an input node in the ref counted input system.
		static bool ClearInputNodeDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier);
};
