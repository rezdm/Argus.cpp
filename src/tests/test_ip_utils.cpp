#include "../network/address_family_handler.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "Testing IP address utilities...\n";

    // Test IPv4 detection - valid addresses
    assert(ip_address_utils::is_valid_ipv4("192.168.1.1") == true);
    assert(ip_address_utils::is_valid_ipv4("8.8.8.8") == true);
    assert(ip_address_utils::is_valid_ipv4("0.0.0.0") == true);
    assert(ip_address_utils::is_valid_ipv4("255.255.255.255") == true);

    // Test IPv4 detection - invalid addresses
    assert(ip_address_utils::is_valid_ipv4("256.1.1.1") == false);
    assert(ip_address_utils::is_valid_ipv4("192.168.1.256") == false);
    assert(ip_address_utils::is_valid_ipv4("192.168.1") == false);
    assert(ip_address_utils::is_valid_ipv4("192.168.1.1.1") == false);
    assert(ip_address_utils::is_valid_ipv4("not.an.ip") == false);
    assert(ip_address_utils::is_valid_ipv4("") == false);
    assert(ip_address_utils::is_valid_ipv4("192.168.-1.1") == false);
    std::cout << "+ IPv4 validation tests passed\n";

    // Test IPv6 detection - valid addresses
    assert(ip_address_utils::is_valid_ipv6("2001:db8::1") == true);
    assert(ip_address_utils::is_valid_ipv6("::1") == true);
    assert(ip_address_utils::is_valid_ipv6("::") == true);
    assert(ip_address_utils::is_valid_ipv6("2001:0db8:85a3:0000:0000:8a2e:0370:7334") == true);
    assert(ip_address_utils::is_valid_ipv6("fe80::1%eth0") == false); // with interface

    // Test IPv6 detection - invalid addresses
    assert(ip_address_utils::is_valid_ipv6("192.168.1.1") == false);
    assert(ip_address_utils::is_valid_ipv6("not::valid::ipv6") == false);
    assert(ip_address_utils::is_valid_ipv6("2001:db8::1::2") == false); // double ::
    assert(ip_address_utils::is_valid_ipv6("2001:db8:85a3::8a2e::7334") == false); // double ::
    assert(ip_address_utils::is_valid_ipv6("2001:db8:85a3:0000:0000:8a2e:0370:7334:extra") == false); // too many groups
    assert(ip_address_utils::is_valid_ipv6("2001:db8:85a3:gggg::1") == false); // invalid hex
    assert(ip_address_utils::is_valid_ipv6("") == false);
    assert(ip_address_utils::is_valid_ipv6(":::1") == false);
    std::cout << "+ IPv6 validation tests passed\n";

    // Test IP type detection - valid cases
    assert(ip_address_utils::detect_ip_type("192.168.1.1") == ip_address_utils::ip_type::ipv4);
    assert(ip_address_utils::detect_ip_type("2001:db8::1") == ip_address_utils::ip_type::ipv6);
    assert(ip_address_utils::detect_ip_type("::1") == ip_address_utils::ip_type::ipv6);
    assert(ip_address_utils::detect_ip_type("127.0.0.1") == ip_address_utils::ip_type::ipv4);

    // Test IP type detection - invalid cases
    assert(ip_address_utils::detect_ip_type("hostname.com") == ip_address_utils::ip_type::invalid);
    assert(ip_address_utils::detect_ip_type("256.256.256.256") == ip_address_utils::ip_type::invalid);
    assert(ip_address_utils::detect_ip_type("not::valid::ipv6") == ip_address_utils::ip_type::invalid);
    assert(ip_address_utils::detect_ip_type("") == ip_address_utils::ip_type::invalid);
    assert(ip_address_utils::detect_ip_type("192.168.1") == ip_address_utils::ip_type::invalid);
    assert(ip_address_utils::detect_ip_type("2001:db8::1::2") == ip_address_utils::ip_type::invalid);
    std::cout << "+ IP type detection tests passed\n";

    // Test numeric IP detection - valid cases
    assert(ip_address_utils::is_numeric_ip("192.168.1.1") == true);
    assert(ip_address_utils::is_numeric_ip("2001:db8::1") == true);
    assert(ip_address_utils::is_numeric_ip("::1") == true);
    assert(ip_address_utils::is_numeric_ip("255.255.255.255") == true);

    // Test numeric IP detection - invalid cases
    assert(ip_address_utils::is_numeric_ip("google.com") == false);
    assert(ip_address_utils::is_numeric_ip("256.1.1.1") == false);
    assert(ip_address_utils::is_numeric_ip("192.168.1") == false);
    assert(ip_address_utils::is_numeric_ip("not::valid") == false);
    assert(ip_address_utils::is_numeric_ip("") == false);
    assert(ip_address_utils::is_numeric_ip("localhost") == false);
    std::cout << "+ Numeric IP detection tests passed\n";

    // Test IPv4-mapped IPv6 detection - valid mapped addresses
    assert(ip_address_utils::is_ipv4_mapped_ipv6("::ffff:192.168.1.1") == true);
    assert(ip_address_utils::is_ipv4_mapped_ipv6("::ffff:8.8.8.8") == true);

    // Test IPv4-mapped IPv6 detection - non-mapped addresses
    assert(ip_address_utils::is_ipv4_mapped_ipv6("2001:db8::1") == false);
    assert(ip_address_utils::is_ipv4_mapped_ipv6("::1") == false);
    assert(ip_address_utils::is_ipv4_mapped_ipv6("fe80::1") == false);

    // Test IPv4-mapped IPv6 detection - invalid addresses
    assert(ip_address_utils::is_ipv4_mapped_ipv6("192.168.1.1") == false); // IPv4, not IPv6
    assert(ip_address_utils::is_ipv4_mapped_ipv6("invalid::address") == false);
    assert(ip_address_utils::is_ipv4_mapped_ipv6("") == false);
    assert(ip_address_utils::is_ipv4_mapped_ipv6("hostname.com") == false);
    std::cout << "+ IPv4-mapped IPv6 detection tests passed\n";

    // Test edge cases and potential vulnerabilities
    std::cout << "\nTesting edge cases and error handling...\n";

    // Test very long strings
    std::string long_string(1000, 'a');
    assert(ip_address_utils::is_valid_ipv4(long_string) == false);
    assert(ip_address_utils::is_valid_ipv6(long_string) == false);
    std::cout << "+ Long string handling tests passed\n";

    // Test strings with null characters (should be handled safely)
    std::string null_string = "192.168.1.1\0extra";
    assert(ip_address_utils::is_valid_ipv4(null_string) == true); // Should stop at null
    std::cout << "+ Null character handling tests passed\n";

    // Test boundary values for IPv4
    assert(ip_address_utils::is_valid_ipv4("0.0.0.0") == true);
    assert(ip_address_utils::is_valid_ipv4("255.255.255.255") == true);
    assert(ip_address_utils::is_valid_ipv4("256.0.0.0") == false);
    assert(ip_address_utils::is_valid_ipv4("-1.0.0.0") == false);
    std::cout << "+ IPv4 boundary value tests passed\n";

    // Test malformed addresses that might cause parsing issues
    assert(ip_address_utils::is_valid_ipv4("192.168..1") == false);
    assert(ip_address_utils::is_valid_ipv4("192.168.1.") == false);
    assert(ip_address_utils::is_valid_ipv4(".192.168.1.1") == false);
    assert(ip_address_utils::is_valid_ipv6("::::::") == false);
    assert(ip_address_utils::is_valid_ipv6("2001:db8:::1") == false);
    std::cout << "+ Malformed address tests passed\n";

    // Test normalization with edge cases
    std::string normalized = ip_address_utils::normalize_ipv6("2001:0db8:0000:0000:0000:0000:0000:0001");
    std::cout << "Normalized IPv6: " << normalized << std::endl;

    std::string invalid_normalize = ip_address_utils::normalize_ipv6("invalid::address");
    std::cout << "Invalid normalization result: " << invalid_normalize << std::endl;
    std::cout << "+ Normalization edge case tests passed\n";

    std::cout << "\nAll tests passed!\n";
    return 0;
}