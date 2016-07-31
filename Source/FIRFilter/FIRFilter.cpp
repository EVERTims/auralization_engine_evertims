#include "FIRFilter.h"


FIRFilter::FIRFilter()
	:
	irSize_(0),
	bufferSize_(0)
{
}

FIRFilter::~FIRFilter()
{
}

void FIRFilter::init(size_t bufferSize, size_t irSize)
{
	assert(bufferSize >= 0);
	assert(irSize >= 0);

	irSize_ = irSize;
	bufferSize_ = bufferSize;
	initialized_ = true;

	nfft_ = (size_t)nextPowerOf2((int)(irSize + bufferSize - 1));
	oouraFFT.init(nfft_);
	ir_.resize(nfft_);
	H_.resize(nfft_ / 2 + 1);
	tail_.resize(nfft_ - bufferSize);

	timeBuffer_.resize(nfft_);
	freqBuffer_.resize(nfft_ / 2 + 1);

    /// DPQ
    // zero-out tail buffer
    memset(tail_.data(), 0, tail_.size() * sizeof(float));
    currentReadPos_ = 0;
}

void FIRFilter::setImpulseResponse(const float* ir)
{
	assert(initialized_);
	if (nfft_ == 0)
		return;

	// zero-padded impulse response
	memcpy(ir_.data(), ir, irSize_ * sizeof(float));

	// compute transfer function
	oouraFFT.fft(ir_.data(), H_.data());
}

void FIRFilter::process(float* in)
{
	assert(initialized_);
	if (nfft_ == 0 || bufferSize_ == 0 || irSize_ == 0)
		return;

	// zero-out time buffer
	memset(timeBuffer_.data(), 0, timeBuffer_.size() * sizeof(float));
	// fft of zero padded input
	memcpy(timeBuffer_.data(), in, bufferSize_ * sizeof(float));
	oouraFFT.fft(timeBuffer_.data(), freqBuffer_.data());

	// multiply and normalize
	auto scale = 2.f / nfft_;
	for (auto i = 0u; i < nfft_ / 2 + 1; ++i)
		freqBuffer_[i] = scale * freqBuffer_[i] * H_[i];

	// ifft of the product
	oouraFFT.ifft(freqBuffer_.data(), timeBuffer_.data());

	// copy to output
	memcpy(in, timeBuffer_.data(), bufferSize_ * sizeof(float));

	// tail
//	for (size_t i = 0; i < tail_.size(); ++i)
//	{
//		in[i] += tail_[i]; // add to output
//		tail_[i] = timeBuffer_[i + bufferSize_]; // buffer
//    
//    }

    ////////////////////////////////////////////////////////////////// DPQ
    // tail
    int tmpReadPos = currentReadPos_;
    
    for (size_t i = 0; i < timeBuffer_.size(); ++i)
    {
        if (i < bufferSize_)
        {
            in[i] += tail_[tmpReadPos]; // add tail to output
            tail_[tmpReadPos] = 0.0;    // empty what's been used
        }
        else
        {
            tail_[tmpReadPos] += timeBuffer_[i]; // buffer in tail
        }
        if (++tmpReadPos >= tail_.size()){
            tmpReadPos = 0;
        }
    }

    currentReadPos_ = currentReadPos_ + bufferSize_;
    if (currentReadPos_ >= tail_.size()) // modulo
    {
        currentReadPos_ = currentReadPos_ - tail_.size();
    }
    
    //////////////////////////////////////////////////////////////////
}

void FIRFilter::reset()
{
	memset(tail_.data(), 0, tail_.size() * sizeof(float));
}
