#include "group.h"
#include <stdexcept>

group::group(int sort_val, const std::string& group_name_val, std::vector<destination> destinations_val)
    : sort_(sort_val), group_name_(group_name_val), destinations_(std::move(destinations_val)) {
    validate_parameters();
}

void group::set_sort(int sort_val) {
    sort_ = sort_val;
}

void group::set_group_name(const std::string& group_name_val) {
    if (group_name_val.empty()) {
        throw std::invalid_argument("Group name cannot be empty");
    }
    group_name_ = group_name_val;
}

void group::set_destinations(const std::vector<destination>& destinations_val) {
    for (const auto& dest : destinations_val) {
        if (!dest.is_valid()) {
            throw std::invalid_argument("Invalid destination in group: " + dest.get_validation_error());
        }
    }
    destinations_ = destinations_val;
}

void group::add_destination(const destination& dest) {
    if (!dest.is_valid()) {
        throw std::invalid_argument("Cannot add invalid destination: " + dest.get_validation_error());
    }
    destinations_.push_back(dest);
}

void group::clear_destinations() {
    destinations_.clear();
}

bool group::is_valid() const {
    if (group_name_.empty()) return false;
    for (const auto& dest : destinations_) {
        if (!dest.is_valid()) return false;
    }
    return true;
}

std::string group::get_validation_error() const {
    if (group_name_.empty()) return "Group name cannot be empty";
    for (size_t i = 0; i < destinations_.size(); ++i) {
        if (!destinations_[i].is_valid()) {
            return "Destination " + std::to_string(i) + " is invalid: " + destinations_[i].get_validation_error();
        }
    }
    return "";
}

void group::validate_parameters() const {
    if (!is_valid()) {
        throw std::invalid_argument("Invalid group parameters: " + get_validation_error());
    }
}