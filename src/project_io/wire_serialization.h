#pragma once

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message_lite.h>

#include <stdexcept>
#include <string>

namespace rhythm::project::detail {
// Reuse the runtime-program codec's deterministic map ordering for authoring
// hashes and stored extension records too. This is stable within our validated
// schema/Protobuf build, not a canonical format across arbitrary future versions.
inline std::string SerializeDeterministically(const google::protobuf::MessageLite& message) {
    std::string bytes;
    google::protobuf::io::StringOutputStream sink(&bytes);
    {
        google::protobuf::io::CodedOutputStream output(&sink);
        output.SetSerializationDeterministic(true);
        if (!message.SerializeToCodedStream(&output))
            throw std::invalid_argument("project.serialize");
    }
    return bytes;
}
}  // namespace rhythm::project::detail
