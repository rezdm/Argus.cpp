#pragma once

#include "destination.h"
#include <string>
#include <vector>

class group {
private:
    int sort_;
    std::string group_name_;
    std::vector<destination> destinations_;

public:
    // Constructors
    group() : sort_(0) {}

    group(int sort_val, const std::string& group_name_val, std::vector<destination> destinations_val);

    // Getters
    [[nodiscard]] int get_sort() const { return sort_; }
    [[nodiscard]] const std::string& get_group_name() const { return group_name_; }
    [[nodiscard]] const std::vector<destination>& get_destinations() const { return destinations_; }
    [[nodiscard]] size_t get_destination_count() const { return destinations_.size(); }

    // Setters with validation
    void set_sort(int sort_val);
    void set_group_name(const std::string& group_name_val);
    void set_destinations(const std::vector<destination>& destinations_val);
    void add_destination(const destination& dest);
    void clear_destinations();

    // Validation methods
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] std::string get_validation_error() const;

private:
    void validate_parameters() const;
};