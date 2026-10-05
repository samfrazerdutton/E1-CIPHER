#include "e1cipher/policy/classification.hpp"

namespace e1cipher::policy {

ClassificationEntry lookup_baseline(std::string_view data_type) noexcept {
    for (const auto& entry : kDataCatalog) {
        if (entry.data_type == data_type) return entry;
    }
    // Fail closed: an unrecognized data type gets Classification::Unknown,
    // never silently reused from the last match or defaulted to something
    // permissive. See docs/architecture-assessment.md and scheduler.cpp's
    // handling of this exact value.
    return ClassificationEntry{data_type, Classification::Unknown, "No catalog entry for this data type."};
}

}  // namespace e1cipher::policy
