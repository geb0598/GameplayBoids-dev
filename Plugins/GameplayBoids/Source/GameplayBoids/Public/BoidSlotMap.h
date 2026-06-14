#pragma once

#include "CoreMinimal.h"

/**
 * @brief Generational slot map: hands out stable handles over a dense array the owner
 * swap-removes in parallel.
 *
 * Stores only the indirection (slot <-> dense index, reuse generations, free list), never the
 * data. On removal the owner swap-removes its own arrays and passes the indices here so the
 * moved element's mapping stays in sync.
 *
 * HandleType must have int32 Slot and uint32 Generation members and default to an unset slot.
 */
template <typename HandleType>
struct TBoidSlotMap
{
	/** Registers an element just appended at DenseIndex and returns a stable handle to it. */
	HandleType Add(int32 DenseIndex)
	{
		int32 Slot;
		if (FreeSlots.Num() > 0)
		{
			Slot = FreeSlots.Pop(EAllowShrinking::No);
			SlotToIndex[Slot] = DenseIndex;
		}
		else
		{
			Slot = SlotToIndex.Add(DenseIndex);
			SlotGeneration.Add(0);
		}
		IndexToSlot.Add(Slot);

		HandleType Handle;
		Handle.Slot = Slot;
		Handle.Generation = SlotGeneration[Slot];
		return Handle;
	}

	/** Mirrors a swap-remove of the dense array: the owner moved LastIndex into DenseIndex. */
	void RemoveAt(int32 DenseIndex, int32 LastIndex)
	{
		const int32 Slot = IndexToSlot[DenseIndex];
		++SlotGeneration[Slot];
		SlotToIndex[Slot] = INDEX_NONE;
		FreeSlots.Add(Slot);

		if (DenseIndex != LastIndex)
		{
			const int32 MovedSlot = IndexToSlot[LastIndex];
			IndexToSlot[DenseIndex] = MovedSlot;
			SlotToIndex[MovedSlot] = DenseIndex;
		}

		IndexToSlot.Pop(EAllowShrinking::No);
	}

	/** Current dense index for a handle, or INDEX_NONE if it is stale. */
	int32 Resolve(const HandleType& Handle) const
	{
		if (Handle.Slot != INDEX_NONE && SlotGeneration.IsValidIndex(Handle.Slot) && SlotGeneration[Handle.Slot] == Handle.Generation)
		{
			return SlotToIndex[Handle.Slot];
		}
		return INDEX_NONE;
	}

	/** Handle for the element currently at DenseIndex; an unset handle if out of range. */
	HandleType MakeHandle(int32 DenseIndex) const
	{
		HandleType Handle;
		if (IndexToSlot.IsValidIndex(DenseIndex))
		{
			Handle.Slot = IndexToSlot[DenseIndex];
			Handle.Generation = SlotGeneration[Handle.Slot];
		}
		return Handle;
	}

	void Reserve(int32 Count)
	{
		SlotToIndex.Reserve(Count);
		SlotGeneration.Reserve(Count);
		IndexToSlot.Reserve(Count);
	}

	void Reset()
	{
		SlotToIndex.Reset();
		SlotGeneration.Reset();
		IndexToSlot.Reset();
		FreeSlots.Reset();
	}

private:
	/** Slot -> current dense index, or INDEX_NONE when the slot is free. */
	TArray<int32> SlotToIndex;

	/** Slot -> reuse counter; a handle is stale once it no longer matches. */
	TArray<uint32> SlotGeneration;

	/** Dense index -> the slot that owns that element. */
	TArray<int32> IndexToSlot;

	/** Freed slots waiting to be reused. */
	TArray<int32> FreeSlots;
};
