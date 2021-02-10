#pragma once

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable) = delete;
};

class FCommandContext : NonCopyable
{
private:
	FCommandContext(D3D12_COMMAND_LIST_TYPE Type);

	void Reset(void);

public:
	~FCommandContext();

	void Initialize();

	uint64_t FinishFrame(bool WaitForCompletion = false);

protected:
	D3D12_COMMAND_LIST_TYPE m_Type;

	LinearAllocator m_CpuLinearAllocator;
	LinearAllocator m_GpuLinearAllocator;
};