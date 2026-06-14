#pragma once

#include "CoreMinimal.h"

/**
 * @brief Generational slot map: hands out stable handles over a dense array the owner
 * swap-removes in parallel.
 *
 * Stores only the indirection (slot <-> dense index, reuse generations, free list), never the
 * data. On removal the owner swap-removes its own arrays and passes the indices here so the
 * moved element's mapping stays in sync.
 */
struct FBoidSlotMap
{
	/** Registers an element just appended at DenseIndex; writes its stable slot and generation. */
	void Add(int32 DenseIndex, int32& OutSlot, uint32& OutGeneration)
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

		OutSlot = Slot;
		OutGeneration = SlotGeneration[Slot];
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
	int32 Resolve(int32 Slot, uint32 Generation) const
	{
		if (Slot != INDEX_NONE && SlotGeneration.IsValidIndex(Slot) && SlotGeneration[Slot] == Generation)
		{
			return SlotToIndex[Slot];
		}
		return INDEX_NONE;
	}

	/** Slot and generation of the element at DenseIndex (to build a handle); false if out of range. */
	bool TryGetSlot(int32 DenseIndex, int32& OutSlot, uint32& OutGeneration) const
	{
		if (IndexToSlot.IsValidIndex(DenseIndex))
		{
			OutSlot = IndexToSlot[DenseIndex];
			OutGeneration = SlotGeneration[OutSlot];
			return true;
		}
		return false;
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
