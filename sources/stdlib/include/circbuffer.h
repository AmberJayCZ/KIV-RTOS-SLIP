#pragma once

#include <hal/intdef.h>
#include "process/spinlock.h"

// Circular buffer queue that does not block upon put/remove operations
template <typename Type, uint32_t Size>
class NonBlocking_Circular_Buffer_Queue {
	private:
		Type buffer[Size];
		uint32_t front, rear;
		spinlock_t lock;
		bool full;

	public:
		NonBlocking_Circular_Buffer_Queue() {
			front = rear = 0;
			full = false;
			spinlock_init(&lock);
		}

		~NonBlocking_Circular_Buffer_Queue() = default;

		// Tests if queue is empty
		bool Empty() const { return front == rear && !full; }

		// Tests if queue is full
		bool Full() const { return full; }

		// Returns number of free slots
		uint32_t Space() const { return full ? 0 : Size - ((rear - front + Size) % Size); }

		// Returns number of occupied slots
		uint32_t Count() const { return Size - Space(); }

		// Puts item into queue, returns false if queue full
		bool Put(const Type item) {
			if (Full())
				return false;

			spinlock_lock(&lock);
			buffer[rear] = item;
			rear = (rear + 1) % Size;
			full = front == rear;
			spinlock_unlock(&lock);

			return true;
		}

		// Removes item from queue, returns false if queue empty
		bool Remove(Type* item) {
			if (Empty())
				return false;

			spinlock_lock(&lock);
			*item = buffer[front];
			front = (front + 1) % Size;
			full = front != rear;
			spinlock_unlock(&lock);

			return true;
		}

		// Puts count items into queue, returns number of items inserted
		uint32_t Multi_Put(const Type* items, const uint32_t count) {
			uint32_t put = 0;

			if (count == 0)
				return 0;

			if (Space() >= count) {
				spinlock_lock(&lock);

				for ( ; put < count; put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
				spinlock_unlock(&lock);
				return put;
			}

			while (put < count) {
				if (Full())
					return put;

				spinlock_lock(&lock);
				uint32_t space = Space();

				for (uint32_t i = 0; i < space && put < count; i++, put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
				spinlock_unlock(&lock);
			}

			return put;
		}

		// Removes items from queue, returns number of items removed
		uint32_t Multi_Remove(Type* buff, const uint32_t count) {
			uint32_t removed = 0;

			if (count == 0)
				return 0;

			if (Count() >= count) {
				spinlock_lock(&lock);

				for ( ; removed < count; removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
				spinlock_unlock(&lock);
				return removed;
			}

			while (removed < count) {
				if (Empty())
					return removed;

				spinlock_lock(&lock);
				uint32_t items = Count();

				for (uint32_t i = 0; i < items && removed < count; i++, removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
				spinlock_unlock(&lock);
			}

			return removed;
		}
};