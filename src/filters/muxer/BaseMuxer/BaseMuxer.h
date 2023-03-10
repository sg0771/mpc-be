/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2022 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "BaseMuxerInputPin.h"
#include "BaseMuxerOutputPin.h"

class CBaseMuxerFilter
	: public CBaseFilter
	, public CCritSec
	, public CAMThread
	, public IMediaSeeking
	, public IDSMPropertyBagImpl
	, public IDSMResourceBagImpl
	, public IDSMChapterBagImpl
{
private:
	std::list<std::unique_ptr<CBaseMuxerInputPin>> m_pInputs;
	std::unique_ptr<CBaseMuxerOutputPin> m_pOutput;
	std::list<std::unique_ptr<CBaseMuxerRawOutputPin>> m_pRawOutputs;

	enum { CMD_EXIT, CMD_RUN };
	DWORD ThreadProc();

	REFERENCE_TIME m_rtCurrent;
	std::list<CBaseMuxerInputPin*> m_pActivePins;

	std::unique_ptr<MuxerPacket> GetPacket();

	void MuxHeaderInternal();
	void MuxPacketInternal(const MuxerPacket* pPacket);
	void MuxFooterInternal();

protected:
	std::list<CBaseMuxerInputPin*> m_pPins;
	CBaseMuxerOutputPin* GetOutputPin() { return m_pOutput.get(); }

	virtual void MuxInit() = 0;

	// only called when the output pin is connected
	virtual void MuxHeader(IBitStream* pBS) {}
	virtual void MuxPacket(IBitStream* pBS, const MuxerPacket* pPacket) {}
	virtual void MuxFooter(IBitStream* pBS) {}

	// always called (useful if the derived class wants to write somewhere else than downstream)
	virtual void MuxHeader() {}
	virtual void MuxPacket(const MuxerPacket* pPacket) {}
	virtual void MuxFooter() {}

	// allows customized pins in derived classes
	virtual HRESULT CreateInput(CStringW name, CBaseMuxerInputPin** ppPin);
	virtual HRESULT CreateRawOutput(CStringW name, CBaseMuxerRawOutputPin** ppPin);

public:
	CBaseMuxerFilter(LPUNKNOWN pUnk, HRESULT* phr, const CLSID& clsid);
	virtual ~CBaseMuxerFilter();

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	void AddInput();

	int GetPinCount();
	CBasePin* GetPin(int n);

	STDMETHODIMP Stop();
	STDMETHODIMP Pause();
	STDMETHODIMP Run(REFERENCE_TIME tStart);

	// IMediaSeeking

	STDMETHODIMP GetCapabilities(DWORD* pCapabilities);
	STDMETHODIMP CheckCapabilities(DWORD* pCapabilities);
	STDMETHODIMP IsFormatSupported(const GUID* pFormat);
	STDMETHODIMP QueryPreferredFormat(GUID* pFormat);
	STDMETHODIMP GetTimeFormat(GUID* pFormat);
	STDMETHODIMP IsUsingTimeFormat(const GUID* pFormat);
	STDMETHODIMP SetTimeFormat(const GUID* pFormat);
	STDMETHODIMP GetDuration(LONGLONG* pDuration);
	STDMETHODIMP GetStopPosition(LONGLONG* pStop);
	STDMETHODIMP GetCurrentPosition(LONGLONG* pCurrent);
	STDMETHODIMP ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat,
								   LONGLONG Source, const GUID* pSourceFormat);
	STDMETHODIMP SetPositions(LONGLONG* pCurrent, DWORD dwCurrentFlags,
							  LONGLONG* pStop, DWORD dwStopFlags);
	STDMETHODIMP GetPositions(LONGLONG* pCurrent, LONGLONG* pStop);
	STDMETHODIMP GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest);
	STDMETHODIMP SetRate(double dRate);
	STDMETHODIMP GetRate(double* pdRate);
	STDMETHODIMP GetPreroll(LONGLONG* pllPreroll);
};
