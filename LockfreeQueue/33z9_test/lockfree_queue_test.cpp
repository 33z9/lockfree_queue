#include "33z9_test/lockfree_queue_test.h"

#include <cstdarg>
#include <mutex>
#include <functional>
#include <Windows.h>

namespace
{
	void OutputText(const char* format, ...)
	{
		char s[512] = {};

		va_list arg;
		va_start(arg, format);
		vsprintf_s(s, format, arg);
		va_end(arg);

		::OutputDebugStringA(s);
	}

	struct Product
	{
		Product()
			: lot_no(0)
			, value(0)
		{

		}

		Product(int32_t lot_no, int32_t value)
			: lot_no(lot_no)
			, value(value)
		{

		}

		Product(const Product& source)
			: lot_no(source.lot_no)
			, value(source.value)
		{

		}

		int32_t lot_no;
		int32_t value;
	};

	class System
	{
	public:
		static const size_t kMaxProduct = 64;

	public:
		System();

		System(const System&) = delete;

		System& operator =(const System&) = delete;

		virtual ~System();

	public:
		std::atomic_bool done_ = false;

		std::mutex job_mutex_;
		std::condition_variable job_condition_;
		std::array<char, sizeof(mmzq::lockfree::Queue<Product>::Element) * kMaxProduct> job_buffer_;
		mmzq::lockfree::Queue<Product> job_;
	};

	System::System()
	{
		job_.Create(job_buffer_.data(), static_cast<uint32_t>(job_buffer_.size()));
	}

	System::~System()
	{
		job_.Destroy();
	}

	namespace Consumer
	{
		void Work(System& ctrl, size_t user_no)
		{
			OutputText("consumer[%zu]: wakeup. \n", user_no);

			Product item;
			while (mmzq::ErrorCode::kSuccess == ctrl.job_.Pop(&item)) {
				OutputText("consumer[%zu]: %02d.%02d \n", user_no, item.lot_no, item.value);

				if (0 < item.value) {
					std::this_thread::sleep_for(std::chrono::milliseconds(item.value));
				}
				else if (item.value < 0) {
				}
			}

			OutputText("consumer[%zu]: sleep. \n", user_no);
		}

		void Exclusive(System& ctrl, size_t user_no)
		{
			std::unique_lock<std::mutex> lock(ctrl.job_mutex_);
			ctrl.job_condition_.wait(lock, [&ctrl] {
				return (ctrl.done_ || 0 < ctrl.job_.Count()) ? true : false;
				});

			Work(ctrl, user_no);

			lock.unlock();
			ctrl.job_condition_.notify_all();
		}

		void Share(System & ctrl, size_t user_no)
		{
			{
				std::unique_lock<std::mutex> lock(ctrl.job_mutex_);
				ctrl.job_condition_.wait(lock, [&ctrl] {
					return (ctrl.done_ || 0 < ctrl.job_.Count()) ? true : false;
					});

				lock.unlock();
				ctrl.job_condition_.notify_all();
			}

			Work(ctrl, user_no);
		}
	}

	namespace Producer
	{
		void Work(System& ctrl, int32_t lot_no, int32_t num)
		{
			OutputText("producer: start lot no.%02d num=%02d. \n", lot_no, num);

			for (int32_t number = 0; number < num; ++number) {
				mmzq::ErrorCode error_code = ctrl.job_.Push(lot_no, number);
				if (FAILED(error_code)) {
					OutputText("producer: fail lot no.%02d num=%02d. \n", lot_no, number);
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(num));

			OutputText("producer: finish lot no.%02d num=%02d. \n", lot_no, num);
		}

		void Exclusive(System& ctrl, int32_t lot_no, int32_t num)
		{
			{
				std::lock_guard<std::mutex> lock(ctrl.job_mutex_);

				Work(ctrl, lot_no, num);
			}

			ctrl.job_condition_.notify_all();
		}

		void Always(System& ctrl, int32_t lot_no, int32_t num)
		{
			Work(ctrl, lot_no, num);

			ctrl.job_condition_.notify_all();
		}
	}
}

namespace mmzq_test
{
	namespace lockfree_queue
	{
		template<typename ProducerFunc, typename ConsumerFunc>
		int32_t Work(ProducerFunc producer_func, ConsumerFunc consumer_func)
		{
			constexpr size_t kMaxConsumer = 3;
			constexpr size_t kMaxLots = 3;
			constexpr size_t kMaxItemsPerLot = 10;

			System ctrl;

			// begin consumer.
			std::array<std::thread, kMaxConsumer> threads;
			for (size_t i = 0; i < kMaxConsumer; ++i) {
				threads[i] = std::thread([&ctrl, &consumer_func](size_t worker_no) {
					while (!ctrl.done_) {
						consumer_func(ctrl, worker_no);
					}
					}, i);
			}

			// production.
			for (int32_t lot_no = 0; lot_no < kMaxLots; ++lot_no) {
				producer_func(ctrl, lot_no, kMaxItemsPerLot);
			}

			// wait.
			while (0 < ctrl.job_.Count()) {
			}

			ctrl.done_ = true;
			ctrl.job_condition_.notify_all();

			// end consumer.
			for (std::thread& t : threads) {
				if (t.joinable()) {
					t.join();
				}
			}

			return 0;
		}

		int32_t Exclusive()
		{
			OutputText("[Producer::Exclusive, Consumer::Exclusive] Start ------------------------------->\n");

			int32_t result = Work(Producer::Exclusive, Consumer::Exclusive);

			OutputText("<------------------------------- End [Producer::Exclusive, Consumer::Exclusive]\n");

			return result;
		}

		int32_t Always()
		{
			OutputText("[Producer::Always, Consumer::Share] Start ------------------------------->\n");

			int32_t result = Work(Producer::Always, Consumer::Share);

			OutputText("<------------------------------- End [Producer::Always, Consumer::Share]\n");

			return result;
		}
	}
}
