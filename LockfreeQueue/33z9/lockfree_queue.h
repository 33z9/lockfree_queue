#ifndef MMZQ_LOCKFREE_QUEUE_H
#define MMZQ_LOCKFREE_QUEUE_H

#include <cinttypes>
#include <atomic>
#include <array>
#include <cassert>

#include "33z9/error_code.h"

namespace mmzq
{
	namespace lockfree
	{
		template<typename T>
		class Queue
		{
		public:
			using value_type = T;
			using pointer = T *;
			using const_pointer = const T*;
			using reference = T &;
			using const_reference = const T &;

			struct Element
			{
				std::atomic_bool stored;
				std::array<char, sizeof(value_type)> buffer;

				template<typename ...Args>
				void Store(Args&& ... args);

				void Load(pointer destination);

				void WaitForStored() const;

				void WaitForEmpty() const;
			};

		public:
			Queue();

			Queue(const Queue&) = delete;

			Queue& operator =(const Queue&) = delete;

			virtual ~Queue();

			/// <summary>
			/// create lockfree queue.
			/// </summary>
			/// <param name="buffer">Do not discard this buffer until you finish using this class.</param>
			/// <param name="buffer_size_in_bytes"></param>
			/// <returns></returns>
			ErrorCode Create(void* buffer, uint32_t buffer_size_in_bytes);

			ErrorCode Destroy();

			bool IsCreated() const;

			template<typename... Args>
			ErrorCode Push(Args&& ... args);

			ErrorCode Pop(pointer destination);

			uint32_t Count() const;

			uint32_t Capacity() const;

		private:
			void* buffer_;
			uint32_t buffer_size_;
			Element* array_;
			uint32_t array_size_;

			std::atomic_bool created_;

			std::atomic_uint32_t head_;
			std::atomic_uint32_t tail_;
			std::atomic_uint32_t count_;
		};

		template<typename T>
		template<typename ...Args>
		void Queue<T>::Element::Store(Args&& ... args)
		{
			assert(!stored);

			::new(buffer.data()) value_type(std::forward<Args>(args)...);
			stored = true;
		}

		template<typename T>
		void Queue<T>::Element::Load(pointer destination)
		{
			assert(stored);

			if (destination) {
				*destination = *reinterpret_cast<pointer>(buffer.data());
			}

			reinterpret_cast<pointer>(buffer.data())->~value_type();
			stored = false;
		}

		template<typename T>
		void Queue<T>::Element::WaitForStored() const
		{
			while (!stored) {
				// spin lock;
			}
		}

		template<typename T>
		void Queue<T>::Element::WaitForEmpty() const
		{
			while (stored) {
				// spin lock;
			}
		}

		template<typename T>
		Queue<T>::Queue()
			: buffer_(nullptr)
			, buffer_size_(0)
			, array_(nullptr)
			, array_size_(0)
			, created_(false)
			, head_(0)
			, tail_(0)
			, count_(0)
		{

		}

		template<typename T>
		Queue<T>::~Queue()
		{
			Destroy();
		}

		template<typename T>
		ErrorCode Queue<T>::Create(void* buffer, uint32_t buffer_size_in_bytes)
		{
			if (IsCreated()) {
				return ErrorCode::kInvalidCall;
			}

			if (nullptr == buffer || buffer_size_in_bytes < sizeof(Element)) {
				return ErrorCode::kInvalidArg;
			}

			buffer_ = buffer;
			buffer_size_ = buffer_size_in_bytes;
			array_ = reinterpret_cast<Element*>(buffer);
			array_size_ = buffer_size_in_bytes / sizeof(Element);

			head_ = 0;
			tail_ = 0;
			count_ = 0;

			// initialize.
			for (uint32_t i = 0; i < array_size_; ++i) {
				array_[i].stored = false;
			}

			created_ = true;

			return ErrorCode::kSuccess;
		}

		template<typename T>
		ErrorCode Queue<T>::Destroy()
		{
			// cleanup.
			while (ErrorCode::kSuccess == Pop(nullptr)) {

			}

			buffer_ = nullptr;
			buffer_size_ = 0;
			array_ = nullptr;
			array_size_ = 0;

			head_ = 0;
			tail_ = 0;
			count_ = 0;

			created_ = false;

			return ErrorCode::kSuccess;
		}

		template<typename T>
		bool Queue<T>::IsCreated() const
		{
			return created_;
		}

		template<typename T>
		template<typename... Args>
		ErrorCode Queue<T>::Push(Args&& ... args)
		{
			if (!IsCreated()) {
				return ErrorCode::kInvalidCall;
			}

			uint32_t index = head_.load(std::memory_order_relaxed);
			uint32_t next_index;

			// reserve.
			do {
				next_index = (index + 1) % array_size_;
				if (next_index == tail_) {
					return ErrorCode::kFail;
				}

			} while (!head_.compare_exchange_weak(index, next_index, std::memory_order_release, std::memory_order_relaxed));

			array_[index].WaitForEmpty();

			++count_;

			array_[index].Store(std::forward<Args>(args)...);

			return ErrorCode::kSuccess;
		}

		template<typename T>
		ErrorCode Queue<T>::Pop(pointer destination)
		{
			if (!IsCreated()) {
				return ErrorCode::kInvalidCall;
			}

			uint32_t index = tail_.load(std::memory_order_relaxed);
			uint32_t next_index;

			// reserve.
			do {
				if (index == head_) {
					return ErrorCode::kFail;
				}
				next_index = (index + 1) % array_size_;

			} while (!tail_.compare_exchange_weak(index, next_index, std::memory_order_release, std::memory_order_relaxed));

			array_[index].WaitForStored();

			--count_;

			array_[index].Load(destination);

			return ErrorCode::kSuccess;
		}

		template<typename T>
		uint32_t Queue<T>::Count() const
		{
			return count_;
		}

		template<typename T>
		uint32_t Queue<T>::Capacity() const
		{
			return array_size_;
		}

	}
}

#endif
