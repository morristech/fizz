/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fizz/client/FizzClient.h>
#include <fizz/client/PskCache.h>
#include <fizz/client/test/Mocks.h>

using namespace folly;
using namespace testing;

namespace fizz {
namespace client {
namespace test {

class MockClientStateMachineInstance : public MockClientStateMachine {
 public:
  MockClientStateMachineInstance() {
    instance = this;
  }
  static MockClientStateMachineInstance* instance;
};
MockClientStateMachineInstance* MockClientStateMachineInstance::instance;

class ActionMoveVisitor : public boost::static_visitor<> {
 public:
  template <typename T>
  void operator()(T&) {}
};

class TestFizzClient : public DelayedDestruction {
 public:
  TestFizzClient() : fizzClient_(state_, queue_, visitor_, this) {}

  State state_;
  IOBufQueue queue_;
  ActionMoveVisitor visitor_;
  FizzClient<ActionMoveVisitor, MockClientStateMachineInstance> fizzClient_;
};

class FizzClientTest : public Test {
 public:
  void SetUp() override {
    context_ = std::make_shared<FizzClientContext>();
    fizzClient_.reset(new TestFizzClient());
  }

 protected:
  std::shared_ptr<FizzClientContext> context_;
  EventBase evb_;
  std::unique_ptr<TestFizzClient, DelayedDestruction::Destructor> fizzClient_;
};

TEST_F(FizzClientTest, TestConnect) {
  EXPECT_CALL(
      *MockClientStateMachineInstance::instance,
      _processConnect(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([] { return Actions(); }));
  const auto sni = std::string("www.example.com");
  fizzClient_->fizzClient_.connect(context_, nullptr, sni, folly::none);
}

TEST_F(FizzClientTest, TestConnectPskIdentity) {
  std::string psk("psk");
  EXPECT_CALL(
      *MockClientStateMachineInstance::instance,
      _processConnect(_, _, _, _, _, _))
      .WillOnce(
          Invoke([psk](
                     const State&,
                     std::shared_ptr<const FizzClientContext> context,
                     std::shared_ptr<const CertificateVerifier> verifier,
                     folly::Optional<std::string> sni,
                     folly::Optional<CachedPsk> cachedPsk,
                     const std::shared_ptr<ClientExtensions>& extensions) {
            EXPECT_TRUE(cachedPsk);
            EXPECT_EQ(cachedPsk->psk, psk);
            EXPECT_EQ(sni, "www.example.com");
            return Actions();
          }));
  const auto sni = std::string("www.example.com");
  CachedPsk cachedPsk;
  cachedPsk.psk = psk;
  fizzClient_->fizzClient_.connect(
      context_, nullptr, sni, std::move(cachedPsk));
}
} // namespace test
} // namespace client
} // namespace fizz
