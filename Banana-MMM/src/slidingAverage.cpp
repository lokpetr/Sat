#include "slidingAverage.h"

// SLIDING AVERAGE ------------------------

template <int N>
SlidingAverage<N>::SlidingAverage() : index(0), count(0) {
	for (int i = 0; i < N; i++)
		samples[i] = 0.0;
}

template <int N>
void SlidingAverage<N>::update(float _new) {
	samples[index] = _new;

	index = (index + 1) % N;

	if (count < N)
		count++;
}

template <int N>
float SlidingAverage<N>::get() const {
	float sum = 0.0;
	for (int i = 0; i < count; i++)
		sum += samples[i];

	return count > 0 ? sum / count : 0.0;
}

template <int N>
float SlidingAverage<N>::calculate(float _new) {
	update(_new);
	return get();
}

// 3-DIMENSIONAL VERSION ------------------

template <int N>
SlidingAverage3D<N>::SlidingAverage3D() {}

template <int N>
void SlidingAverage3D<N>::update(float newX, float newY, float newZ) {
	xAverage.update(newX);
	yAverage.update(newY);
	zAverage.update(newZ);
}

template <int N>
void SlidingAverage3D<N>::update(xyz_data<float> _new) {
	update(_new.x, _new.y, _new.z);
}

template <int N>
float SlidingAverage3D<N>::getX() const {
	return xAverage.get();
}

template <int N>
float SlidingAverage3D<N>::getY() const {
	return yAverage.get();
}

template <int N>
float SlidingAverage3D<N>::getZ() const {
	return zAverage.get();
}

template <int N>
xyz_data<float> SlidingAverage3D<N>::get() const {
	return xyz_data(getX(), getY(), getZ());
}

template <int N>
xyz_data<float> SlidingAverage3D<N>::calculate(xyz_data<float> _new) {
	update(_new);
	return get();
}

template <int N>
xyz_data<float> SlidingAverage3D<N>::calculate(float newX, float newY, float newZ) {
	update(newX, newY, newZ);
	return get();
}

// N-DIMENSIONAL VERSION ------------------

template <int N, int D>
SlidingAverageND<N, D>::SlidingAverageND() {}

template <int N, int D>
void SlidingAverageND<N, D>::update(float newValues[D]) {
	for (int i = 0; i < D; i++)
		averages[i].update(newValues[i]);
}

template <int N, int D>
void SlidingAverageND<N, D>::calculate(float newValues[D], float output[D]) {
	update(newValues);
	for (int i = 0; i < D; i++)
		output[i] = averages[i].get();
}

template <int N, int D>
void SlidingAverageND<N, D>::get(float output[D]) const {
	for (int i = 0; i < D; i++)
		output[i] = get(i);
}

template <int N, int D>
float SlidingAverageND<N, D>::get(uint8_t idx) const {
	return averages[idx].get();
}

template class SlidingAverage<5>;
template class SlidingAverage3D<5>;
template class SlidingAverageND<5, 4>;
