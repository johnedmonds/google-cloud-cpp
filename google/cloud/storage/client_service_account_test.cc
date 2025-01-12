// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::oauth2::CreateAnonymousCredentials;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;

/**
 * Test the Projects.serviceAccount-related functions in storage::Client.
 */
class ServiceAccountTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options = ClientOptions(CreateAnonymousCredentials());
};

TEST_F(ServiceAccountTest, GetProjectServiceAccount) {
  ServiceAccount expected =
      internal::ServiceAccountParser::FromString(
          R"""({"email_address": "test-service-account@test-domain.com"})""")
          .value();

  EXPECT_CALL(*mock, GetServiceAccount(_))
      .WillOnce(Return(StatusOr<ServiceAccount>(TransientError())))
      .WillOnce(Invoke(
          [&expected](internal::GetProjectServiceAccountRequest const& r) {
            EXPECT_EQ("test-project", r.project_id());

            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<ServiceAccount> actual =
      client.GetServiceAccountForProject("test-project");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountTooManyFailures) {
  testing::TooManyFailuresStatusTest<ServiceAccount>(
      mock, EXPECT_CALL(*mock, GetServiceAccount(_)),
      [](Client& client) {
        return client.GetServiceAccountForProject("test-project").status();
      },
      "GetServiceAccount");
}

TEST_F(ServiceAccountTest, GetProjectServiceAccountPermanentFailure) {
  testing::PermanentFailureStatusTest<ServiceAccount>(
      *client, EXPECT_CALL(*mock, GetServiceAccount(_)),
      [](Client& client) {
        return client.GetServiceAccountForProject("test-project").status();
      },
      "GetServiceAccount");
}

TEST_F(ServiceAccountTest, CreateHmacKey) {
  internal::CreateHmacKeyResponse expected =
      internal::CreateHmacKeyResponse::FromHttpResponse(
          R"""({"secretKey": "dGVzdC1zZWNyZXQ=", "resource": {}})""")
          .value();

  EXPECT_CALL(*mock, CreateHmacKey(_))
      .WillOnce(
          Return(StatusOr<internal::CreateHmacKeyResponse>(TransientError())))
      .WillOnce(Invoke([&expected](internal::CreateHmacKeyRequest const& r) {
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-service-account", r.service_account());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<std::pair<HmacKeyMetadata, std::string>> actual =
      client.CreateHmacKey("test-service-account",
                           OverrideDefaultProject("test-project"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.metadata, actual->first);
  EXPECT_EQ(expected.secret, actual->second);
}

TEST_F(ServiceAccountTest, CreateHmacKeyTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::CreateHmacKeyResponse>(
      mock, EXPECT_CALL(*mock, CreateHmacKey(_)),
      [](Client& client) {
        return client.CreateHmacKey("test-service-account").status();
      },
      "CreateHmacKey");
}

TEST_F(ServiceAccountTest, CreateHmacKeyPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::CreateHmacKeyResponse>(
      *client, EXPECT_CALL(*mock, CreateHmacKey(_)),
      [](Client& client) {
        return client.CreateHmacKey("test-service-account").status();
      },
      "CreateHmacKey");
}

TEST_F(ServiceAccountTest, DeleteHmacKey) {
  EXPECT_CALL(*mock, DeleteHmacKey(_))
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce(Invoke([](internal::DeleteHmacKeyRequest const& r) {
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  Status actual = client.DeleteHmacKey("test-access-id-1",
                                       OverrideDefaultProject("test-project"));
  ASSERT_STATUS_OK(actual);
}

TEST_F(ServiceAccountTest, DeleteHmacKeyTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteHmacKey(_)),
      [](Client& client) { return client.DeleteHmacKey("test-access-id"); },
      "DeleteHmacKey");
}

TEST_F(ServiceAccountTest, DeleteHmacKeyPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteHmacKey(_)),
      [](Client& client) { return client.DeleteHmacKey("test-access-id"); },
      "DeleteHmacKey");
}

TEST_F(ServiceAccountTest, GetHmacKey) {
  HmacKeyMetadata expected =
      internal::HmacKeyMetadataParser::FromString(
          R"""({"accessId": "test-access-id-1", "state": "ACTIVE"})""")
          .value();

  EXPECT_CALL(*mock, GetHmacKey(_))
      .WillOnce(Return(StatusOr<HmacKeyMetadata>(TransientError())))
      .WillOnce(Invoke([&expected](internal::GetHmacKeyRequest const& r) {
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<HmacKeyMetadata> actual = client.GetHmacKey(
      "test-access-id-1", OverrideDefaultProject("test-project"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.access_id(), actual->access_id());
  EXPECT_EQ(expected.state(), actual->state());
}

TEST_F(ServiceAccountTest, GetHmacKeyTooManyFailures) {
  testing::TooManyFailuresStatusTest<HmacKeyMetadata>(
      mock, EXPECT_CALL(*mock, GetHmacKey(_)),
      [](Client& client) {
        return client.GetHmacKey("test-access-id").status();
      },
      "GetHmacKey");
}

TEST_F(ServiceAccountTest, GetHmacKeyPermanentFailure) {
  testing::PermanentFailureStatusTest<HmacKeyMetadata>(
      *client, EXPECT_CALL(*mock, GetHmacKey(_)),
      [](Client& client) {
        return client.GetHmacKey("test-access-id").status();
      },
      "GetHmacKey");
}

TEST_F(ServiceAccountTest, UpdateHmacKey) {
  HmacKeyMetadata expected =
      internal::HmacKeyMetadataParser::FromString(
          R"""({"accessId": "test-access-id-1", "state": "ACTIVE"})""")
          .value();

  EXPECT_CALL(*mock, UpdateHmacKey(_))
      .WillOnce(Return(StatusOr<HmacKeyMetadata>(TransientError())))
      .WillOnce(Invoke([&expected](internal::UpdateHmacKeyRequest const& r) {
        EXPECT_EQ("test-project", r.project_id());
        EXPECT_EQ("test-access-id-1", r.access_id());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<HmacKeyMetadata> actual = client.UpdateHmacKey(
      "test-access-id-1", HmacKeyMetadata().set_state("ACTIVE"),
      OverrideDefaultProject("test-project"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected.access_id(), actual->access_id());
  EXPECT_EQ(expected.state(), actual->state());
}

TEST_F(ServiceAccountTest, UpdateHmacKeyTooManyFailures) {
  testing::TooManyFailuresStatusTest<HmacKeyMetadata>(
      mock, EXPECT_CALL(*mock, UpdateHmacKey(_)),
      [](Client& client) {
        return client.UpdateHmacKey("test-access-id", HmacKeyMetadata())
            .status();
      },
      "UpdateHmacKey");
}

TEST_F(ServiceAccountTest, UpdateHmacKeyPermanentFailure) {
  testing::PermanentFailureStatusTest<HmacKeyMetadata>(
      *client, EXPECT_CALL(*mock, UpdateHmacKey(_)),
      [](Client& client) {
        return client.UpdateHmacKey("test-access-id", HmacKeyMetadata())
            .status();
      },
      "UpdateHmacKey");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
