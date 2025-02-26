/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/
#include <miopen/tensor.hpp>

#include <miopen/errors.hpp>
#include <miopen/logger.hpp>
#include <miopen/tensor_layout.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>

namespace miopen {

TensorDescriptor::TensorDescriptor() : packed(true) {}

TensorDescriptor::TensorDescriptor(miopenDataType_t t, std::initializer_list<std::size_t> plens)
    : lens(plens), packed(true), type(t)
{
    this->CalculateVectorLength();
    this->CalculateStrides();
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   miopenTensorLayout_t playout,
                                   std::initializer_list<std::size_t> plens)
    : lens(plens), packed(true), type(t), tensorLayout(playout)
{
    this->CalculateVectorLength();
    this->CalculateStrides();
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   miopenTensorLayout_t playout,
                                   std::vector<std::size_t> plens)
    : lens(plens), packed(true), type(t), tensorLayout(playout)
{
    this->CalculateVectorLength();
    this->CalculateStrides();
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   std::initializer_list<std::size_t> plens,
                                   std::initializer_list<std::size_t> pstrides)
    : lens(plens), strides(pstrides), type(t)
{
    this->CalculateVectorLength();
    packed = (this->GetElementSize() == this->GetElementSpace());
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t, const int* plens, int size)
    : lens(plens, plens + size), packed(true), type(t)
{
    if(!std::all_of(plens, plens + size, [](int x) { return x >= 0; }))
        MIOPEN_THROW("Invalid length. Length must be greater than 0.");
    this->CalculateVectorLength();
    this->CalculateStrides();
}
TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   const int* plens,
                                   const int* pstrides,
                                   int size)
    : lens(plens, plens + size), strides(pstrides, pstrides + size), type(t)
{
    if(!std::all_of(plens, plens + size, [](int x) { return x >= 0; }))
        MIOPEN_THROW("Invalid length. Length must be greater than 0.");
    if(!std::all_of(pstrides, pstrides + size, [](int x) { return x >= 0; }))
        MIOPEN_THROW("Invalid strides. Strides must be greater than 0.");
    this->CalculateVectorLength();
    packed = (this->GetElementSize() == this->GetElementSpace());
}
TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   miopenTensorLayout_t playout,
                                   const int* plens,
                                   int size)
    : lens(plens, plens + size), packed(true), type(t), tensorLayout(playout)
{
    if(!std::all_of(plens, plens + size, [](int x) { return x >= 0; }))
        MIOPEN_THROW("Invalid length. Length must be greater than 0.");
    this->CalculateVectorLength();
    this->CalculateStrides();
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   std::vector<std::size_t> lens_in,
                                   std::vector<std::size_t> strides_in)
    : lens(std::move(lens_in)), strides(std::move(strides_in)), type(t)
{
    this->CalculateVectorLength();
    packed = (this->GetElementSize() == this->GetElementSpace());
}

TensorDescriptor::TensorDescriptor(miopenDataType_t t,
                                   miopenTensorLayout_t layout_in,
                                   std::vector<std::size_t> lens_in,
                                   std::vector<std::size_t> strides_in)
    : lens(std::move(lens_in)), strides(std::move(strides_in)), type(t), tensorLayout(layout_in)
{
    this->CalculateVectorLength();
    packed = (this->GetElementSize() == this->GetElementSpace());
}

void TensorDescriptor::CalculateStrides()
{
    strides.clear();
    strides.resize(lens.size(), 0);
    if(strides.empty())
        return;
    if(tensorLayout == miopenTensorNCHWc4 || tensorLayout == miopenTensorNCHWc8)
    {
        lens[1] /= vector_length;
    }
    else if(tensorLayout == miopenTensorCHWNc4 || tensorLayout == miopenTensorCHWNc8)
    {
        lens[0] /= vector_length;
    }

    strides.back() = vector_length;
    std::partial_sum(
        lens.rbegin(), lens.rend() - 1, strides.rbegin() + 1, std::multiplies<std::size_t>());
    for(int i = 0; i < strides.size() - 1; i++)
        strides[i] *= vector_length;
}

void TensorDescriptor::CalculateVectorLength()
{
    vector_length =
        ((tensorLayout == miopenTensorCHWNc8 || tensorLayout == miopenTensorNCHWc8)
             ? 8
             : ((tensorLayout == miopenTensorCHWNc4 || tensorLayout == miopenTensorNCHWc4) ? 4
                                                                                           : 1));
}

bool TensorDescriptor::IsVectorized() const { return vector_length > 1; }

const std::vector<std::size_t>& TensorDescriptor::GetLengths() const { return lens; }
const std::vector<std::size_t>& TensorDescriptor::GetStrides() const { return strides; }
int TensorDescriptor::GetSize() const
{
    assert(lens.size() == strides.size());
    return lens.size();
}
std::size_t TensorDescriptor::GetElementSize() const
{
    assert(lens.size() == strides.size());
    return std::accumulate(lens.begin(), lens.end(), vector_length, std::multiplies<std::size_t>());
}
miopenDataType_t TensorDescriptor::GetType() const { return this->type; }
miopenTensorLayout_t TensorDescriptor::GetLayout_t() const { return this->tensorLayout; }
std::string TensorDescriptor::GetLayout_str() const
{
    switch(this->tensorLayout)
    {
    case miopenTensorNCHW: return "NCHW";
    case miopenTensorNHWC: return "NHWC";
    case miopenTensorNCHWc4:
    case miopenTensorNCHWc8: return "NCHWc";
    case miopenTensorCHWN: return "CHWN";
    case miopenTensorCHWNc4:
    case miopenTensorCHWNc8: return "CHWNc";
    case miopenTensorNCDHW: return "NCDHW";
    case miopenTensorNDHWC: return "NDHWC";
    }
    return "Unknown tensor layout";
}
std::size_t TensorDescriptor::GetVectorLength() const { return this->vector_length; }

std::size_t TensorDescriptor::GetIndex(std::initializer_list<int> l) const
{
    // l is in NCHW order (MIOpen implicit logic)
    if(this->GetLayout_str() == "CHWNc")
    {
        assert(l.size() - 1 <= this->GetSize());
        std::initializer_list<int> l_chwn{
            *(l.begin()), *(l.begin() + 2), *(l.begin() + 3), *(l.begin() + 4), *(l.begin() + 1)};
        return std::inner_product(l_chwn.begin() + 1,
                                  l_chwn.end(),
                                  strides.begin(),
                                  static_cast<std::size_t>(*(l_chwn.begin())));
    }
    else
    {
        if(!this->IsVectorized())
        {
            assert(l.size() <= this->GetSize());
            return std::inner_product(l.begin(), l.end(), strides.begin(), std::size_t{0});
        }
        else
        {
            assert(l.size() - 1 <= this->GetSize());
            return std::inner_product(
                l.begin() + 1, l.end(), strides.begin(), static_cast<std::size_t>(*(l.begin())));
        }
    }
}

std::size_t TensorDescriptor::GetElementSpace() const
{
    std::vector<std::size_t> maxIndices(lens.size());
    std::transform(lens.begin(),
                   lens.end(),
                   std::vector<std::size_t>(lens.size(), 1).begin(),
                   maxIndices.begin(),
                   std::minus<std::size_t>());
    return std::inner_product(
               maxIndices.begin(), maxIndices.end(), strides.begin(), std::size_t{0}) +
           vector_length;
}

bool TensorDescriptor::IsPossibleLayout(const std::string& labels, const std::string& layout) const
{
    std::vector<size_t> derived_strides;
    tensor_layout_to_strides(lens, labels, layout, derived_strides);
    return derived_strides == strides;
}

std::size_t TensorDescriptor::GetNumBytes() const
{
    std::size_t typesize = 0;
    switch(this->type)
    {
    case miopenInt8x4:
    case miopenInt8: typesize = 1; break;
    case miopenBFloat16:
    case miopenHalf: typesize = 2; break;
    case miopenInt32:
    case miopenFloat: typesize = 4; break;
    case miopenDouble: typesize = 8; break;
    }
    return typesize * this->GetElementSpace();
}

bool TensorDescriptor::IsPacked() const { return this->packed; }

bool TensorDescriptor::operator==(const TensorDescriptor& rhs) const
{
    assert(this->lens.size() == rhs.strides.size());
    return this->type == rhs.type && this->lens == rhs.lens && this->strides == rhs.strides;
}

bool TensorDescriptor::operator!=(const TensorDescriptor& rhs) const { return !(*this == rhs); }

bool TensorDescriptor::operator<(const TensorDescriptor& rhs) const
{
    return (std::tie(this->GetLengths(), this->GetStrides()) <
            std::tie(rhs.GetLengths(), rhs.GetStrides()));
}

bool TensorDescriptor::operator>(const TensorDescriptor& rhs) const
{
    return (std::tie(this->GetLengths(), this->GetStrides()) >
            std::tie(rhs.GetLengths(), rhs.GetStrides()));
}

std::string TensorDescriptor::ToString() const
{
    std::string result;
    if(this->lens.empty())
        return result;
    for(auto i : this->lens)
    {
        result += std::to_string(i) + ", ";
    }
    return result.substr(0, result.length() - 2);
}

std::ostream& operator<<(std::ostream& stream, const TensorDescriptor& t)
{
    return LogRange(stream, t.lens, ", ");
}

void to_json(nlohmann::json& j, const TensorDescriptor& descriptor)
{
    j = nlohmann::json{
        {"lengths", descriptor.lens},
        {"strides", descriptor.strides},
        {"packed", descriptor.packed},
        {"type", descriptor.type},
    };
}

void from_json(const nlohmann::json& j, TensorDescriptor& descriptor)
{
    j.at("lengths").get_to(descriptor.lens);
    j.at("strides").get_to(descriptor.strides);
    j.at("packed").get_to(descriptor.packed);
    j.at("type").get_to(descriptor.type);
}

} // namespace miopen

// TODO(paul): Remove
MIOPEN_EXPORT
int miopenGetTensorIndex(miopenTensorDescriptor_t tensorDesc, std::initializer_list<int> indices)
{
    return miopen::deref(tensorDesc).GetIndex(indices);
}
