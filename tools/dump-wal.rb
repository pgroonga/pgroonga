#!/usr/bin/env ruby

require "optparse"
require "ostruct"
require "pp"

require "msgpack"

options = OpenStruct.new
options.start_block = 1
options.start_offset = 0
option_parser = OptionParser.new
option_parser.on("--start-block=BLOCK",
                 Integer,
                 "Start from the BLOCK") do |block|
  options.start_block = block
end
option_parser.on("--start-offset=OFFSET",
                 Integer,
                 "Start from the OFFSET in BLOCK") do |offset|
  options.start_offset = offset
end
option_parser.parse!(ARGV)

page_header_size = [
  8, # PageXLogRecPtr pd_lsn;
  2, # uint16		pd_checksum;
  2, # uint16		pd_flags;
  2, # LocationIndex pd_lower;
  2, # LocationIndex pd_upper;
  2, # LocationIndex pd_special;
  2, # uint16		pd_pagesize_version;
  4, # TransactionId pd_prune_xid;
].sum

def parse_page_header(data)
  names = [
    "lsn_xlogid",
    "lsn_xrecoff",
    "checksum",
    "flags",
    "lower",
    "upper",
    "special",
    "pagesize_version",
    "prune_xid",
  ]
  Hash[names.zip(data.unpack("LLSSSSSSL"))]
end

def parse_meta_page_special(data)
  case data.bytesize
  when 12
    Hash[["next", "max", "version"].zip(data.unpack("LLC"))]
  else
    Hash[["next", "max", "version", "end"].zip(data.unpack("LLLL"))]
  end
end

block_size = 8192
unpacker = MessagePack::Unpacker.new
i = 0
while page = ARGF.read(block_size)
  header = parse_page_header(page.byteslice(0, page_header_size))
  if i == 0
    puts("Meta:")
    puts("  Header:")
    header.each do |key, value|
      puts("    #{key}: #{value}")
    end
    meta_page_special = page.byteslice(header["special"],
                                       block_size - header["special"])
    parse_meta_page_special(meta_page_special).each do |key, value|
      puts("  #{key}: #{value}")
    end
  else
    need_output = true
    data = page.byteslice(page_header_size,
                          header["lower"] - page_header_size)
    if i < options.start_block
      need_output = false
    elsif i == options.start_block
      data = data[options.start_offset, data.bytesize - options.start_offset]
    end
    if need_output
      puts("Page#{i}")
      header.each do |key, value|
        puts("  #{key}: #{value}")
      end
      unpacker.feed_each(data) do |object|
        puts(PP.pp(object, ""))
      end
    end
  end
  i += 1
end
