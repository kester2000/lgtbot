// Copyright (c) 2023-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#pragma once

#include <mutex>

template <typename T>
class LockWrapper : private T
{
    template <typename TQ> // type with qualifier
    class LockGuard
    {
      public:
        LockGuard(TQ& v, std::mutex& m) : v_(v), l_(m) {}

        TQ& operator*() { return v_; }
        const TQ& operator*() const { return v_; }

        TQ* operator->() { return &v_; }
        const TQ* operator->() const { return &v_; }

      private:
        TQ& v_;
        std::lock_guard<std::mutex> l_;
    };

  public:
    using T::T;
    LockWrapper(const T& v) : T(v) {}
    LockWrapper(T&& v) : T(std::move(v)) {}
    LockGuard<T> Lock() { return LockGuard<T>(*this, m_); }
    LockGuard<const T> Lock() const { return LockGuard<const T>(*this, m_); }

  private:
    mutable std::mutex m_;
};
