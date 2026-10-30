#include <hal/intdef.h>

/* Circular buffer queue that can block upon put/remove operations */
template <typename Type, uint32_t Size>
class Blocking_Circular_Buffer_Queue {
	private:
		Type buffer[Size];
		uint32_t front, rear;
		bool full;

	public:
		Blocking_Circular_Buffer_Queue() {
			front = rear = 0;
			full = false;
		}

		~Blocking_Circular_Buffer_Queue() = default;

		bool Empty() const { return front == rear && !full; }

		bool Full() const { return full; }

		/* Returns number of free slots */
		uint32_t Space() const { return full ? 0 : Size - ((rear - front + Size) % Size); }

		/* Returns number of occupied slots */
		uint32_t Count() const { return Size - Space(); }

		/* Puts item into queue, blocks if full */
		void Put(const Type item) {
			while (Full()) {
				; /* Active wait for slot */
			}

			buffer[rear] = item;
			rear = (rear + 1) % Size;
			full = front == rear;
		}

		/* Removes item from queue, blocks if empty */
		Type Remove() {
			while (Empty()) {
				; /* Active wait for item */
			}

			Type item = buffer[front];
			front = (front + 1) % Size;
			full = front != rear;
			return item;
		}

		/* Puts count items into queue in largest possible chunks, blocks if full */
		void Multi_Put(const Type* items, uint32_t count) {
			uint32_t put = 0;

			if (Space() >= count) {
				for ( ; put < count; put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
				return;
			}

			while (put < count) {
				while (Full()) {
					; /* Active wait for slots */
				}

				uint32_t space = Space();

				for (uint32_t i = 0; i < space && put < count; i++, put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
			}
		}

		/* Removes items from queue in largest possible chunks, blocks if empty */
		void Multi_Remove(Type* buff, uint32_t count) {
			uint32_t removed = 0;

			if (Count() >= count) {
				for ( ; removed < count; removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
				return;
			}

			while (removed < count) {
				while (Empty()) {
					; /* Active wait for items */
				}

				uint32_t items = Count();

				for (uint32_t i = 0; i < items && removed < count; i++, removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
			}
		}
};



/* Circular buffer queue that does not block upon put/remove operations */
template <typename Type, uint32_t Size>
class NonBlocking_Circular_Buffer_Queue {
	private:
		Type buffer[Size];
		uint32_t front, rear;
		bool full;

	public:
		Blocking_Circular_Buffer_Queue() {
			front = rear = 0;
			full = false;
		}

		~Blocking_Circular_Buffer_Queue() = default;

		bool Empty() const { return front == rear && !full; }

		bool Full() const { return full; }

		/* Returns number of free slots */
		uint32_t Space() const { return full ? 0 : Size - ((rear - front + Size) % Size); }

		/* Returns number of occupied slots */
		uint32_t Count() const { return Size - Space(); }

		/* Puts item into queue, returns false if queue full */
		bool Put(const Type item) {
			if (Full())
				return false;

			buffer[rear] = item;
			rear = (rear + 1) % Size;
			full = front == rear;

			return true;
		}

		/* Removes item from queue, returns false if queue empty */
		bool Remove(Type* item) {
			if (Empty())
				return false;

			*item = buffer[front];
			front = (front + 1) % Size;
			full = front != rear;
			return true;
		}

		/* Puts count items into queue, returns number of items inserted */
		uint32_t Multi_Put(const Type* items, uint32_t count) {
			uint32_t put = 0;

			if (Space() >= count) {
				for ( ; put < count; put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
				return;
			}

			while (put < count) {
				if (Full())
					return put;

				uint32_t space = Space();

				for (uint32_t i = 0; i < space && put < count; i++, put++) {
					buffer[rear] = items[put];
					rear = (rear + 1) % Size;
				}

				full = front == rear;
			}
		}

		/* Removes items from queue, returns number of items removed */
		uint32_t Multi_Remove(Type* buff, uint32_t count) {
			uint32_t removed = 0;

			if (Count() >= count) {
				for ( ; removed < count; removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
				return;
			}

			while (removed < count) {
				if (Empty())
					return removed;

				uint32_t items = Count();

				for (uint32_t i = 0; i < items && removed < count; i++, removed++) {
					buff[removed] = buffer[front];
					front = (front + 1) % Size;
				}

				full = front != rear;
			}
		}
};