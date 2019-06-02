#ifndef MMZQ_ERROR_CODE_H
#define MMZQ_ERROR_CODE_H

namespace mmzq
{
	enum class ErrorCode : int32_t
	{
		kSuccess = 0,

		kFail = -1,
		kInvalidArg = -2,
		kInvalidCall = -3,
	};
}

#endif
