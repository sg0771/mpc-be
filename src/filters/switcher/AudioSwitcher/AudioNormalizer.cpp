/*
 * (C) 2014-2022 see Authors.txt
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

#include "stdafx.h"
#include "AudioNormalizer.h"

//
// CAudioNormalizer
//

CAudioNormalizer::CAudioNormalizer()
{
	memset(m_prediction, 7, sizeof(m_prediction));
}

CAudioNormalizer::~CAudioNormalizer()
{
}

int CAudioNormalizer::ProcessInternal(float *samples, unsigned numsamples, unsigned nch)
{
	if (m_vol <= 0) {
		m_vol = 1;
	}

	const size_t allsamples = numsamples * nch;

	float max_peak = 0.0f;
	for (size_t k = 0; k < allsamples; k++) {
		max_peak = std::max(max_peak , std::abs(samples[k]));
	}

	int tries = 0;

redo:
	const double factor = m_vol != m_stepping_vol ? (double)m_vol / (double)(m_stepping_vol) : 1.0;
	const double highest = (double)max_peak * factor * 32768;

	if (highest > 30000.0) {
		if (m_vol > m_stepping) {
			m_vol -= m_stepping;
		} else {
			m_vol = 1;
		}
		tries++;
		if (tries < 1024) {
			goto redo;
		} else {
			return numsamples;
		}
	}

	if (highest > 10.0) {
		const double t = (m_level << 15) / 100.0;
		if (highest > t) {
			if (m_prediction[m_predictor] > 0) {
				m_prediction[m_predictor] = m_prediction[m_predictor] - 1;
			} else {
				m_prediction[m_predictor] = 0;
			}
			const int predict = m_prediction[m_predictor];
			m_predictor >>= 1;
			m_rising = 0;
			if (predict <= 7 && m_vol > 1) {
				m_vol--;
			}
		} else { // highest <= t
			if (m_prediction[m_predictor] < 15) {
				m_prediction[m_predictor] = m_prediction[m_predictor] + 1;
			} else {
				m_prediction[m_predictor] = 15;
			}
			const int predict = m_prediction[m_predictor];
			m_predictor = (m_predictor >> 1) | 0x800;
			if (predict >= 8) {
				m_vol += (++m_rising < 128) ? 1 : m_stepping;
			}
		}
	}

	for (size_t k = 0; k < allsamples; k++) {
		samples[k] *= factor;
	}

	if (m_boost == false && m_vol > m_stepping_vol) {
		m_vol = m_stepping_vol;
	}

	return numsamples;
}

int CAudioNormalizer::Process(float *samples, unsigned numsamples, unsigned nch)
{
	int ret = 0;

	while (numsamples > 0) {
		const unsigned process = std::min(numsamples, 512u);

		ret += ProcessInternal(samples, process, nch);
		numsamples -= process;
		samples += (process * nch);
	}

	return ret;
}

void CAudioNormalizer::SetParam(int Level, bool Boost, int Steping)
{
	m_level = Level;
	m_boost = Boost;
	if (m_stepping != Steping) {
		m_vol = (m_vol << Steping) / (1 << m_stepping);

		m_stepping = Steping;
		m_stepping_vol = 1 << m_stepping;
	}
}

//
// CAudioAutoVolume
//

CAudioAutoVolume::CAudioAutoVolume()
{
	for (size_t i = 0; i < std::size(m_smooth); i++)
	{
		m_smooth[i] = SmoothNew(100);
	}
}

CAudioAutoVolume::~CAudioAutoVolume()
{
	for (size_t i = 0; i < std::size(m_smooth); i++)
	{
		if (m_smooth[i]) SmoothDelete(m_smooth[i]);
	}
}

int CAudioAutoVolume::Process(float *samples, int numsamples, int nch)
{
	const size_t allsamples = numsamples * nch;
	for (size_t k = 0; k < allsamples; k++) {
		samples[k] *= INT16_MAX;
	}

	double level = -1.0;

	calc_power_level(samples, numsamples, nch);
	{
		int channel = 0;

		level = -1.0;
		for (channel = 0; channel < nch; ++channel)
		{
			double channel_level = SmoothGetMax(m_smooth[channel]);

			if (channel_level > level) level = channel_level;
		}
	}

	if (level > m_silence_level)
	{
		double gain = m_normalize_level / level;

		if (gain > m_max_mult) gain = m_max_mult;

		adjust_gain(samples, numsamples, nch, gain);
	}

	for (size_t k = 0; k < allsamples; k++) {
		samples[k] /= INT16_MAX;
	}

	return numsamples;
}

void CAudioAutoVolume::calc_power_level(float *samples, int numsamples, int nch)
{
	int channel = 0;
	int i = 0;
	double sum[8];
	float *data = samples;

	for (channel = 0; channel < nch; ++channel)
	{
		sum[channel] = 0.0;
	}

	for (i = 0, channel = 0; i < numsamples * nch; ++i, ++data)
	{
		double sample = *data;
		double temp = 0.0;

		if (m_do_compress)
		{
			if (sample > m_cutoff) sample = m_cutoff + (sample - m_cutoff) / m_degree;
		}

		temp = sample*sample;

		sum[channel] += temp;

		++channel;
		channel = channel % nch;
	}

	{
		static const double NORMAL = 1.0 / (double)INT16_MAX;
		static const double NORMAL_SQUARED = NORMAL * NORMAL;
		double channel_length = 2.0 / (numsamples * nch);

		for (channel = 0; channel < nch; ++channel)
		{
			double level = sum[channel] * channel_length * NORMAL_SQUARED;

			SmoothAddSample(m_smooth[channel], sqrt(level));
		}
	}
}

void CAudioAutoVolume::adjust_gain(float *samples, int numsamples, int nch, double gain)
{
	float *data = samples;
	int i = 0;
#define NO_GAIN 0.01

	if (gain >= 1.0 - NO_GAIN && gain <= 1.0 + NO_GAIN) return;

	for (i = 0; i < numsamples * nch; ++i, ++data)
	{
		double samp = (double)*data;

		if (m_do_compress)
		{
			if (samp > m_cutoff) samp = m_cutoff + (samp - m_cutoff) / m_degree;
		}
		samp *= gain;
		*data = (float)std::clamp(samp, (double)INT16_MIN, (double)INT16_MAX);
	}
}

CAudioAutoVolume::smooth_t *CAudioAutoVolume::SmoothNew(int size)
{
	smooth_t * sm = (smooth_t *)malloc(sizeof(smooth_t));
	if (sm == nullptr) return nullptr;

	ZeroMemory(sm, sizeof(smooth_t));

	sm->data = (double *)malloc(size * sizeof(double));
	if (sm->data == nullptr)
	{
		free(sm);
		return nullptr;
	}
	ZeroMemory(sm->data, size * sizeof(double));

	sm->size = size;
	sm->current = sm->used = 0;
	sm->max = 0.0;
	return sm;
}

void CAudioAutoVolume::SmoothDelete(smooth_t *del)
{
	if (del == nullptr)
		return;

	if (del->data != nullptr) free(del->data);

	free(del);
}

void CAudioAutoVolume::SmoothAddSample(smooth_t *sm, double sample)
{
	if (sm == nullptr)	return;

	sm->data[sm->current] = sample;

	++sm->current;

	if (sm->current > sm->used) ++sm->used;
	if (sm->current >= sm->size) sm->current %= sm->size;
}

double CAudioAutoVolume::SmoothGetMax(smooth_t *sm)
{
	if (sm == nullptr) return -1.0;

	{
		int i = 0;
		double smoothed = 0.0;

		for (i = 0; i < sm->used; ++i) smoothed += sm->data[i];
		smoothed = smoothed / sm->used;

		if (sm->used < sm->size) return (smoothed * sm->used + m_normalize_level * (sm->size - sm->used)) / sm->size;

		if (sm->max < smoothed) sm->max = smoothed;
	}

	return sm->max;
}
