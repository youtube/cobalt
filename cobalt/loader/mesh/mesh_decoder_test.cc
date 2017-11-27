// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/mesh/mesh_decoder.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/loader/mesh/mesh_projection.h"
#include "cobalt/render_tree/mesh.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace loader {
namespace mesh {

namespace {

const char kTestMeshbox[] = "projection.box";

struct MockMeshDecoderCallback {
  void SuccessCallback(const scoped_refptr<MeshProjection>& value) {
    mesh_projection = value;
  }

  MOCK_METHOD1(ErrorCallback, void(const std::string& message));

  scoped_refptr<MeshProjection> mesh_projection;
};

class MockMeshDecoder : public Decoder {
 public:
  MockMeshDecoder();
  ~MockMeshDecoder() override {}

  void DecodeChunk(const char* data, size_t size) override;

  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

  scoped_refptr<MeshProjection> GetMeshProjection();

  void ExpectCallWithError(const std::string& message);

 protected:
  ::testing::StrictMock<MockMeshDecoderCallback> mesh_decoder_callback_;
  render_tree::ResourceProviderStub resource_provider_;
  scoped_ptr<Decoder> mesh_decoder_;
};

MockMeshDecoder::MockMeshDecoder() {
  mesh_decoder_.reset(new mesh::MeshDecoder(
      &resource_provider_,
      base::Bind(&MockMeshDecoderCallback::SuccessCallback,
                 base::Unretained(&mesh_decoder_callback_)),
      base::Bind(&MockMeshDecoderCallback::ErrorCallback,
                 base::Unretained(&mesh_decoder_callback_))));
}

void MockMeshDecoder::DecodeChunk(const char* data, size_t size) {
  mesh_decoder_->DecodeChunk(data, size);
}

void MockMeshDecoder::Finish() { mesh_decoder_->Finish(); }

bool MockMeshDecoder::Suspend() { return mesh_decoder_->Suspend(); }

void MockMeshDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  mesh_decoder_->Resume(resource_provider);
}

scoped_refptr<MeshProjection> MockMeshDecoder::GetMeshProjection() {
  return mesh_decoder_callback_.mesh_projection;
}

void MockMeshDecoder::ExpectCallWithError(const std::string& message) {
  EXPECT_CALL(mesh_decoder_callback_, ErrorCallback(message));
}

FilePath GetTestMeshPath(const char* file_name) {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("loader"))
      .Append(FILE_PATH_LITERAL("testdata"))
      .Append(FILE_PATH_LITERAL(file_name));
}

std::vector<uint8> GetMeshData(const FilePath& file_path) {
  int64 size;
  std::vector<uint8> mesh_data;

  bool success = file_util::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0L);

  mesh_data.resize(static_cast<size_t>(size));

  int num_of_bytes =
      file_util::ReadFile(file_path, reinterpret_cast<char*>(&mesh_data[0]),
                          static_cast<int>(size));

  CHECK_EQ(static_cast<size_t>(num_of_bytes), mesh_data.size())
      << "Could not read '" << file_path.value() << "'.";
  return mesh_data;
}

}  // namespace

// Test that we can decode a mesh received in one chunk.
TEST(MeshDecoderTest, DecodeMesh) {
  MockMeshDecoder mesh_decoder;

  std::vector<uint8> mesh_data = GetMeshData(GetTestMeshPath(kTestMeshbox));
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[0]),
                           mesh_data.size());
  mesh_decoder.Finish();

  EXPECT_TRUE(mesh_decoder.GetMeshProjection());

  scoped_refptr<render_tree::MeshStub> mesh(
      make_scoped_refptr(base::polymorphic_downcast<render_tree::MeshStub*>(
          mesh_decoder.GetMeshProjection()->GetMesh(0).get())));

  EXPECT_EQ(13054UL, mesh->GetVertices().size());

  render_tree::Mesh::Vertex v0 = mesh->GetVertices()[0];
  render_tree::Mesh::Vertex v577 = mesh->GetVertices()[577];
  render_tree::Mesh::Vertex v13053 = mesh->GetVertices()[13053];

  EXPECT_NEAR(v0.x, -1.0f, 0.0001);
  EXPECT_NEAR(v0.y, -1.0f, 0.0001);
  EXPECT_NEAR(v0.z, 1.0f, 0.0001);
  EXPECT_NEAR(v0.u, 0.497917f, 0.0001);
  EXPECT_NEAR(v0.v, 0.00185186f, 0.0001);

  EXPECT_NEAR(v577.x, -1.0f, 0.0001);
  EXPECT_NEAR(v577.y, 0.0f, 0.0001);
  EXPECT_NEAR(v577.z, 0.4375f, 0.0001);
  EXPECT_NEAR(v577.u, 0.25, 0.0001);
  EXPECT_NEAR(v577.v, 0.0807092f, 0.0001);

  EXPECT_NEAR(v13053.x, 1.0f, 0.0001);
  EXPECT_NEAR(v13053.y, -1.0f, 0.0001);
  EXPECT_NEAR(v13053.z, -1.0f, 0.0001);
  EXPECT_NEAR(v13053.u, 0.997917f, 0.0001);
  EXPECT_NEAR(v13053.v, 0.00185186f, 0.0001);
}

// Test that we can decode a mesh received in multiple chunks.
TEST(MeshDecoderTest, DecodeMeshWithMultipleChunks) {
  MockMeshDecoder mesh_decoder;

  std::vector<uint8> mesh_data = GetMeshData(GetTestMeshPath(kTestMeshbox));
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[0]), 4);
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[4]), 2);
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[6]), 94);
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[100]), 100);
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&mesh_data[200]),
                           mesh_data.size() - 200);
  mesh_decoder.Finish();

  EXPECT_TRUE(mesh_decoder.GetMeshProjection());
}

// Test that decoder signals error on empty buffer.
TEST(MeshDecoderTest, DoNotDecodeEmptyProjectionBoxBuffer) {
  MockMeshDecoder mesh_decoder;
  mesh_decoder.ExpectCallWithError(
      "MeshDecoder passed an empty buffer, cannot decode.");

  std::vector<uint8> empty_mesh_data(1);
  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&empty_mesh_data[0]), 0);
  mesh_decoder.Finish();

  EXPECT_FALSE(mesh_decoder.GetMeshProjection());
}

// Test that an invalid projection box triggers error.
TEST(MeshDecoderTest, DoNotDecodeInvalidProjectionBox) {
  MockMeshDecoder mesh_decoder;
  mesh_decoder.ExpectCallWithError(
      "MeshDecoder passed an invalid mesh projection box.");

  std::vector<uint8> buffer(40, static_cast<uint8>(5));

  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&buffer[0]), buffer.size());
  mesh_decoder.Finish();

  EXPECT_FALSE(mesh_decoder.GetMeshProjection());
}

// Test that a projection box with invalid (empty) meshes triggers error.
TEST(MeshDecoderTest, DoNotDecodeProjectionBoxWithInvalidMeshes) {
  MockMeshDecoder mesh_decoder;
  mesh_decoder.ExpectCallWithError(
      "MeshDecoder passed an invalid mesh projection box.");

  std::vector<uint8> buffer;
  size_t mshp_box_size_offset = buffer.size();
  buffer.insert(buffer.end(), 4, 0);          // Fullbox version, flags.
  buffer.insert(buffer.end(), 4, 0);          // CRC (ignored).
  buffer.push_back(static_cast<uint8>('r'));  // Compression type.
  buffer.push_back(static_cast<uint8>('a'));
  buffer.push_back(static_cast<uint8>('w'));
  buffer.push_back(static_cast<uint8>(' '));
  size_t mesh_box_size_offset = buffer.size();
  buffer.insert(buffer.end(), 4, 0);          // Mesh Box size (set below).
  buffer.push_back(static_cast<uint8>('m'));  // Mesh box type.
  buffer.push_back(static_cast<uint8>('e'));
  buffer.push_back(static_cast<uint8>('s'));
  buffer.push_back(static_cast<uint8>('h'));
  // Empty meshes (with 0 coordinates, 0 vertices and 0 vertex lists).
  buffer.insert(buffer.end(), 12, 0);

  // Write mshp box size.
  buffer[mshp_box_size_offset + 0] = (buffer.size() >> 24) & 0xff;
  buffer[mshp_box_size_offset + 1] = (buffer.size() >> 16) & 0xff;
  buffer[mshp_box_size_offset + 2] = (buffer.size() >> 8) & 0xff;
  buffer[mshp_box_size_offset + 3] = buffer.size() & 0xff;
  // Write mshp box size.
  size_t mesh_box_size = buffer.size() - mesh_box_size_offset;
  buffer[mesh_box_size_offset + 0] = (mesh_box_size >> 24) & 0xff;
  buffer[mesh_box_size_offset + 1] = (mesh_box_size >> 16) & 0xff;
  buffer[mesh_box_size_offset + 2] = (mesh_box_size >> 8) & 0xff;
  buffer[mesh_box_size_offset + 3] = mesh_box_size & 0xff;

  mesh_decoder.DecodeChunk(reinterpret_cast<char*>(&buffer[0]), buffer.size());
  mesh_decoder.Finish();
}

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt
