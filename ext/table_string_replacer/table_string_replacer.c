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

// Fast serialized PHP string replacement with improved memory handling
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
    
    // Early optimization: if old and new are identical, return original
    if (old_len == new_len && memcmp(old, new, old_len) == 0) {
        return rb_str_dup(orig_str);
    }
    
    // Estimate result size more accurately to avoid reallocations
    long max_replacements = orig_len / (old_len > 0 ? old_len : 1);
    long size_diff = new_len - old_len;
    long estimated_result_len = orig_len + (size_diff > 0 ? size_diff * max_replacements : 0) + 128;
    
    // Pre-allocate result buffer
    VALUE result = rb_str_new(NULL, estimated_result_len);
    char *res_ptr = RSTRING_PTR(result);
    long res_len = 0;
    
    long i = 0;
    while (i < orig_len) {
        // Check for serialized string marker
        if (i + 2 < orig_len && orig[i] == 's' && orig[i+1] == ':') {
            char *endptr;
            long len_pos = i + 2;
            
            // Extract length value more safely
            if (len_pos < orig_len) {
                long len_val = strtol(orig + len_pos, &endptr, 10);
                
                // Verify we found a valid PHP serialized string
                if (endptr && *endptr == ':' && (endptr+1) < orig + orig_len && *(endptr+1) == '"') {
                    long content_start = (endptr + 2) - orig;
                    
                    // Make sure content_start is within bounds
                    if (content_start < orig_len) {
                        // Only search within the actual serialized string content
                        long search_limit = content_start + len_val;
                        if (search_limit > orig_len) search_limit = orig_len;
                        
                        char *found = strcasestr(orig + content_start, old);
                        
                        // Found match within the serialized string content
                        if (found && found < orig + search_limit) {
                            // Verify we have enough space in result buffer (resize if needed)
                            long needed_len = res_len + (found - (orig + i)) + new_len + 100;
                            if (needed_len > estimated_result_len) {
                                rb_str_resize(result, needed_len * 2);
                                res_ptr = RSTRING_PTR(result);
                                estimated_result_len = needed_len * 2;
                            }
                        
                            // Calculate new serialized string length
                            long new_len_val = len_val - old_len + new_len;
                            
                            // Update the serialized string length indicator
                            char len_buf[32];
                            int len_digits = snprintf(len_buf, sizeof(len_buf), "s:%ld:", new_len_val);
                            
                            // Copy prefix up to the 's:' marker
                            memcpy(res_ptr + res_len, orig + i, 2);
                            res_len += 2;
                            
                            // Copy new length
                            memcpy(res_ptr + res_len, len_buf + 2, len_digits - 2);
                            res_len += len_digits - 2;
                            
                            // Copy from length end to the found match position
                            long pre_len = found - (orig + content_start);
                            memcpy(res_ptr + res_len, endptr, pre_len + 2);
                            res_len += pre_len + 2;
                            
                            // Copy the new replacement string
                            memcpy(res_ptr + res_len, new, new_len);
                            res_len += new_len;
                            
                            // Skip to after the replacement point
                            i = found - orig + old_len;
                            continue;
                        }
                    }
                }
            }
        }
        
        // If we didn't perform a replacement, copy the current character
        if (res_len >= estimated_result_len) {
            rb_str_resize(result, estimated_result_len * 2);
            res_ptr = RSTRING_PTR(result);
            estimated_result_len *= 2;
        }
        res_ptr[res_len++] = orig[i++];
    }
    
    // Set final string length and terminate
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