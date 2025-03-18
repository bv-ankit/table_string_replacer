require 'mkmf'

# Check for strcasestr function
have_func('strcasestr')
have_header('string.h')
have_header('stdlib.h')
have_header('ctype.h')

# Add optimization flags for best performance
$CFLAGS << ' -O3 -fPIC -fomit-frame-pointer -ffast-math -funroll-loops'

# Platform-specific optimizations
if RUBY_PLATFORM =~ /x86_64|i386/
  $CFLAGS << ' -msse4.2' if try_cflags('-msse4.2')
end

# Thread safety flags
$CFLAGS << ' -pthread' unless $CFLAGS.include?('-pthread')

create_makefile('table_string_replacer/table_string_replacer') 