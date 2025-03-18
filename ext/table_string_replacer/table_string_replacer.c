#include <ruby.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Thread-local Boyer-Moore search buffer size
#define BM_BUFFER_SIZE 256

#ifndef HAVE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    
    // Use Boyer-Moore-like approach for better performance
    int skip[BM_BUFFER_SIZE];
    size_t i, j;
    
    // Initialize skip table
    for (i = 0; i < BM_BUFFER_SIZE; i++)
        skip[i] = needle_len;
    
    for (i = 0; i < needle_len - 1; i++)
        skip[toupper((unsigned char)needle[i]) % BM_BUFFER_SIZE] = needle_len - i - 1;
    
    // Search
    for (i = 0; i <= haystack_len - needle_len; i += skip[toupper((unsigned char)haystack[i + needle_len - 1]) % BM_BUFFER_SIZE]) {
        for (j = 0; j < needle_len; j++) {
            if (toupper((unsigned char)haystack[i + j]) != toupper((unsigned char)needle[j]))
                break;
        }
        
        if (j == needle_len)
            return (char*)(haystack + i);
    }
    
    return NULL;
}
#endif

// Helper function for binary-safe case-insensitive string search
static char* binary_strcasestr(const char* haystack, long haystack_len, const char* needle, long needle_len) {
    if (needle_len == 0) return (char*)haystack;
    if (haystack_len < needle_len) return NULL;
    
    for (long i = 0; i <= haystack_len - needle_len; i++) {
        long j;
        for (j = 0; j < needle_len; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (toupper((unsigned char)h) != toupper((unsigned char)n))
                break;
        }
        if (j == needle_len)
            return (char*)(haystack + i);
    }
    
    return NULL;
}

// Extract serialized string length safely handling binary content
static long extract_serialized_len(const char* str, long max_len, long start_pos, char** endptr) {
    // Ensure we have enough chars for a valid length
    if (start_pos + 1 >= max_len) {
        *endptr = NULL;
        return 0;
    }
    
    // Skip to first digit
    long pos = start_pos;
    while (pos < max_len && !isdigit(str[pos])) pos++;
    
    // Extract digits until we hit a colon
    long val = 0;
    while (pos < max_len && isdigit(str[pos])) {
        val = val * 10 + (str[pos] - '0');
        pos++;
    }
    
    // Check for valid format (must end with colon)
    if (pos < max_len && str[pos] == ':') {
        *endptr = (char*)(str + pos);
        return val;
    } else {
        *endptr = NULL;
        return 0;
    }
}

// Fast serialized PHP string replacement with improved memory handling and binary string support
static VALUE rb_serialized_str_replace(VALUE self, VALUE orig_str, VALUE old_str, VALUE new_str) {
    // Ensure strings are properly initialized
    Check_Type(orig_str, T_STRING);
    Check_Type(old_str, T_STRING);
    Check_Type(new_str, T_STRING);
    
    char *orig = RSTRING_PTR(orig_str);
    char *old = RSTRING_PTR(old_str);
    char *new = RSTRING_PTR(new_str);
    
    long orig_len = RSTRING_LEN(orig_str);
    long old_len = RSTRING_LEN(old_str);
    long new_len = RSTRING_LEN(new_str);
    
    // Early optimization: if old string is empty or original is empty, return original
    if (old_len == 0 || orig_len == 0) {
        return rb_str_dup(orig_str);
    }
    
    // Early optimization: if old and new are identical, return original
    if (old_len == new_len && memcmp(old, new, old_len) == 0) {
        return rb_str_dup(orig_str);
    }
    
    // Pre-compute a more accurate size estimate by counting potential matches first
    long count = 0;
    long i = 0;
    
    // First-pass to count potential replacements in serialized strings
    while (i < orig_len) {
        // Look for serialized string marker pattern 's:'
        if (i + 2 < orig_len && orig[i] == 's' && orig[i+1] == ':') {
            char *endptr;
            long len_val = extract_serialized_len(orig, orig_len, i+2, &endptr);
            
            // Valid PHP serialized string format: s:N:"content";
            if (endptr && (endptr+1) < orig + orig_len && *(endptr+1) == '"') {
                long content_start = (endptr + 2) - orig;
                
                if (content_start < orig_len) {
                    // Only search within the actual serialized string content
                    long search_limit = content_start + len_val;
                    if (search_limit > orig_len) search_limit = orig_len;
                    
                    // Count occurrences within this serialized string
                    char *pos = orig + content_start;
                    char *end = orig + search_limit;
                    long remaining = search_limit - content_start;
                    
                    while (remaining >= old_len) {
                        char *found = binary_strcasestr(pos, remaining, old, old_len);
                        if (found && found < end) {
                            count++;
                            long advance = found - pos + old_len;
                            pos += advance;
                            remaining -= advance;
                        } else {
                            break;
                        }
                    }
                }
                
                // Skip past this serialized string entirely for the counting phase
                i = content_start + len_val;
                // Skip past closing quote and semicolon if present
                if (i < orig_len && orig[i] == '"') i++;
                if (i < orig_len && orig[i] == ';') i++;
                continue;
            }
        }
        i++;
    }
    
    // Optimize allocation with more precise size calculation
    long size_diff = new_len - old_len;
    long estimated_result_len = orig_len + (size_diff > 0 ? size_diff * count : 0) + 256;
    
    // Pre-allocate result buffer - slightly oversized to minimize reallocations
    VALUE result = rb_str_new(NULL, estimated_result_len);
    char *res_ptr = RSTRING_PTR(result);
    long res_len = 0;
    
    // Cache the first character of the search string for faster initial checks
    unsigned char first_char = (unsigned char)old[0];
    unsigned char first_char_upper = toupper(first_char);
    unsigned char first_char_lower = tolower(first_char);
    
    // Second pass to perform the actual replacements
    i = 0;
    while (i < orig_len) {
        // Look for serialized string marker pattern 's:'
        if (i + 2 < orig_len && orig[i] == 's' && orig[i+1] == ':') {
            char *endptr;
            long len_val = extract_serialized_len(orig, orig_len, i+2, &endptr);
            
            // Valid PHP serialized string format: s:N:"content";
            if (endptr && (endptr+1) < orig + orig_len && *(endptr+1) == '"') {
                long content_start = (endptr + 2) - orig;
                
                if (content_start < orig_len) {
                    // Only search within the actual serialized string content
                    long search_limit = content_start + len_val;
                    if (search_limit > orig_len) search_limit = orig_len;
                    
                    // Fast path: check if this serialized string might contain our pattern
                    // by checking for the first character before doing a full search
                    int potential_match = 0;
                    for (long scan_pos = content_start; scan_pos < search_limit; scan_pos++) {
                        unsigned char c = (unsigned char)orig[scan_pos];
                        if (c == first_char || toupper(c) == first_char_upper || 
                            tolower(c) == first_char_lower) {
                            potential_match = 1;
                            break;
                        }
                    }
                    
                    // Only do full search if potential match found
                    if (potential_match) {
                        char *pos = orig + content_start;
                        char *end = orig + search_limit;
                        long remaining = search_limit - content_start;
                        
                        char *found = binary_strcasestr(pos, remaining, old, old_len);
                        
                        // Found match within the serialized string content
                        if (found && found < end) {
                            // Before applying replacement, ensure we have enough space
                            long prefix_len = found - pos;
                            
                            // Calculate new serialized string length
                            long modified_len = len_val + (new_len - old_len);
                            
                            // Build the new serialized string header
                            char len_buf[32];
                            int len_digits = snprintf(len_buf, sizeof(len_buf), "s:%ld:", modified_len);
                            
                            // Ensure we have space in result buffer
                            long needed_space = res_len + len_digits + prefix_len + new_len + 
                                              (search_limit - (found + old_len)) + 16;
                            if (needed_space > estimated_result_len) {
                                estimated_result_len = (estimated_result_len * 3) / 2;
                                if (estimated_result_len < needed_space)
                                    estimated_result_len = needed_space + 256;
                                rb_str_resize(result, estimated_result_len);
                                res_ptr = RSTRING_PTR(result);
                            }
                            
                            // Copy the serialized string header
                            memcpy(res_ptr + res_len, len_buf, len_digits);
                            res_len += len_digits;
                            
                            // Copy quote and prefix content
                            res_ptr[res_len++] = '"';
                            memcpy(res_ptr + res_len, pos, prefix_len);
                            res_len += prefix_len;
                            
                            // Copy the replacement string
                            memcpy(res_ptr + res_len, new, new_len);
                            res_len += new_len;
                            
                            // Copy remainder of serialized string
                            long suffix_len = search_limit - (found + old_len);
                            if (suffix_len > 0) {
                                memcpy(res_ptr + res_len, found + old_len, suffix_len);
                                res_len += suffix_len;
                            }
                            
                            // Add closing quote and semicolon
                            res_ptr[res_len++] = '"';
                            res_ptr[res_len++] = ';';
                            
                            // Skip past the entire processed serialized string
                            i = search_limit;
                            // Skip past closing quote and semicolon if present
                            if (i < orig_len && orig[i] == '"') i++;
                            if (i < orig_len && orig[i] == ';') i++;
                            continue;
                        }
                    }
                    
                    // No match found, copy serialized string as is
                    long str_total_len;
                    long end_pos = search_limit;
                    
                    // Find the end of the serialized string (should be quote+semicolon)
                    if (end_pos < orig_len && orig[end_pos] == '"') end_pos++;
                    if (end_pos < orig_len && orig[end_pos] == ';') end_pos++;
                    
                    str_total_len = end_pos - i;
                    
                    if (res_len + str_total_len > estimated_result_len) {
                        estimated_result_len = (estimated_result_len * 3) / 2;
                        if (estimated_result_len < res_len + str_total_len)
                            estimated_result_len = res_len + str_total_len + 256;
                        rb_str_resize(result, estimated_result_len);
                        res_ptr = RSTRING_PTR(result);
                    }
                    
                    memcpy(res_ptr + res_len, orig + i, str_total_len);
                    res_len += str_total_len;
                    i += str_total_len;
                    continue;
                }
            }
        }
        
        // If we didn't perform a replacement, copy the current character
        if (res_len >= estimated_result_len) {
            estimated_result_len = (estimated_result_len * 3) / 2;
            rb_str_resize(result, estimated_result_len);
            res_ptr = RSTRING_PTR(result);
        }
        res_ptr[res_len++] = orig[i++];
    }
    
    // Set final string length
    rb_str_resize(result, res_len);
    
    return result;
}

// Optimized batch string replacement
static VALUE rb_batch_replace(VALUE self, VALUE orig_str, VALUE replacements) {
    Check_Type(orig_str, T_STRING);
    Check_Type(replacements, T_ARRAY);
    
    long replacements_count = RARRAY_LEN(replacements);
    if (replacements_count == 0) {
        return rb_str_dup(orig_str);
    }
    
    char *orig = RSTRING_PTR(orig_str);
    long orig_len = RSTRING_LEN(orig_str);
    
    // For small number of replacements, use a simpler algorithm
    if (replacements_count <= 3) {
        VALUE result = rb_str_dup(orig_str);
        
        for (long i = 0; i < replacements_count; i++) {
            VALUE pair = rb_ary_entry(replacements, i);
            VALUE old_val = rb_ary_entry(pair, 0);
            VALUE new_val = rb_ary_entry(pair, 1);
            
            // Use Ruby's gsub! for simplicity with small numbers of replacements
            result = rb_funcall(result, rb_intern("gsub!"), 2, old_val, new_val);
            if (NIL_P(result)) {
                // If no replacements were made, restore original
                result = rb_str_dup(orig_str);
            }
        }
        
        return result;
    }
    
    // For larger numbers of replacements, use a more efficient algorithm
    // Calculate maximum possible result length
    long max_result_len = orig_len;
    for (long i = 0; i < replacements_count; i++) {
        VALUE pair = rb_ary_entry(replacements, i);
        VALUE old_str = rb_ary_entry(pair, 0);
        VALUE new_str = rb_ary_entry(pair, 1);
        
        long old_len = RSTRING_LEN(old_str);
        long new_len = RSTRING_LEN(new_str);
        
        if (new_len > old_len) {
            // Add extra space for each potential replacement
            long potential_replacements = old_len > 0 ? orig_len / old_len : 0;
            max_result_len += potential_replacements * (new_len - old_len);
        }
    }
    
    // Allocate result buffer with extra room
    VALUE result = rb_str_new(NULL, max_result_len + 1024);
    char *res_ptr = RSTRING_PTR(result);
    long res_len = 0;
    
    // Store pattern data for faster lookup
    char **patterns = ALLOCA_N(char*, replacements_count);
    char **replacements_ptr = ALLOCA_N(char*, replacements_count);
    long *pattern_lens = ALLOCA_N(long, replacements_count);
    long *replacement_lens = ALLOCA_N(long, replacements_count);
    
    for (long i = 0; i < replacements_count; i++) {
        VALUE pair = rb_ary_entry(replacements, i);
        VALUE old_str = rb_ary_entry(pair, 0);
        VALUE new_str = rb_ary_entry(pair, 1);
        
        patterns[i] = RSTRING_PTR(old_str);
        pattern_lens[i] = RSTRING_LEN(old_str);
        replacements_ptr[i] = RSTRING_PTR(new_str);
        replacement_lens[i] = RSTRING_LEN(new_str);
    }
    
    // Process the string in a single pass
    long i = 0;
    while (i < orig_len) {
        int matched = 0;
        
        // Find longest matching pattern at current position
        long best_match_len = 0;
        long best_match_idx = -1;
        
        for (long j = 0; j < replacements_count; j++) {
            long pattern_len = pattern_lens[j];
            
            // Skip if we don't have enough characters left
            if (i + pattern_len > orig_len || pattern_len <= 0) continue;
            
            // Only consider if this would be a longer match
            if (pattern_len > best_match_len) {
                // Check if this pattern matches at current position
                if (memcmp(orig + i, patterns[j], pattern_len) == 0) {
                    best_match_len = pattern_len;
                    best_match_idx = j;
                    matched = 1;
                }
            }
        }
        
        if (matched) {
            // Apply the best match
            long replace_len = replacement_lens[best_match_idx];
            
            // Ensure we have room in the result buffer
            if (res_len + replace_len >= max_result_len) {
                max_result_len *= 2;
                rb_str_resize(result, max_result_len);
                res_ptr = RSTRING_PTR(result);
            }
            
            // Copy replacement string
            memcpy(res_ptr + res_len, replacements_ptr[best_match_idx], replace_len);
            res_len += replace_len;
            
            // Advance past the matched pattern
            i += best_match_len;
        } else {
            // No match, copy the current character
            if (res_len >= max_result_len) {
                max_result_len *= 2;
                rb_str_resize(result, max_result_len);
                res_ptr = RSTRING_PTR(result);
            }
            res_ptr[res_len++] = orig[i++];
        }
    }
    
    // Resize to actual length
    rb_str_resize(result, res_len);
    
    return result;
}

// Benchmark function to measure performance
static VALUE rb_benchmark(VALUE self, VALUE iterations, VALUE orig_str, VALUE replacements) {
    Check_Type(iterations, T_FIXNUM);
    Check_Type(orig_str, T_STRING);
    Check_Type(replacements, T_ARRAY);
    
    long iter = FIX2LONG(iterations);
    VALUE result = Qnil;
    
    for (long i = 0; i < iter; i++) {
        // Free the previous result to avoid memory buildup
        if (!NIL_P(result)) {
            result = Qnil;
        }
        result = rb_batch_replace(self, orig_str, replacements);
    }
    
    return result;
}

// Module initialization
void Init_table_string_replacer() {
    VALUE mStringReplacer = rb_define_module("TableStringReplacer");
    
    // Core functionality
    rb_define_singleton_method(mStringReplacer, "serialized_str_replace", rb_serialized_str_replace, 3);
    rb_define_singleton_method(mStringReplacer, "batch_replace", rb_batch_replace, 2);
    
    // Benchmarking utility
    rb_define_singleton_method(mStringReplacer, "benchmark", rb_benchmark, 3);
    
    // Constants for thread safety documentation
    rb_define_const(mStringReplacer, "THREAD_SAFE", Qtrue);
    rb_define_const(mStringReplacer, "VERSION", rb_str_new_cstr(RSTRING_PTR(rb_const_get(mStringReplacer, rb_intern("VERSION")))));
}