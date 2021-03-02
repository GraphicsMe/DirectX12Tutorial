#pragma once

#include "D3D12Resource.h"

class FGpuBuffer : public FD3D12Resource
{
public:
	virtual ~FGpuBuffer() { Destroy(); }

	void Create(const std::wstring& Name, uint32_t NumElements, uint32_t ElementSize, const void* InitData = nullptr);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t BaseVertexIndex = 0) const
	{
		size_t Offset = BaseVertexIndex * m_ElementSize;
		return VertexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize);
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit = false) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t StartIndex = 0) const
	{
		size_t Offset = StartIndex * m_ElementSize;
		return IndexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize == 4);
	}
	
	uint32_t GetBufferSize() const { return m_BufferSize; }
	uint32_t GetElementCount() const { return m_ElementCount; }
	uint32_t GetElementSize() const { return m_ElementSize; }

protected:
	D3D12_RESOURCE_DESC DescribeBuffer();

protected:
	D3D12_CPU_DESCRIPTOR_HANDLE m_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV;

	uint32_t m_BufferSize;
	uint32_t m_ElementCount;
	uint32_t m_ElementSize;
	D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

class FConstBuffer : public FGpuBuffer
{
public:
	FConstBuffer() : m_MappedData(nullptr) {}
	~FConstBuffer() { Unmap(); }

	void CreateUpload(const std::wstring& Name, uint32_t Size);
	D3D12_CPU_DESCRIPTOR_HANDLE CreateConstantBufferView(uint32_t Offset, uint32_t Size);

	void* Map();
	void Unmap();

private:
	void* m_MappedData;
};