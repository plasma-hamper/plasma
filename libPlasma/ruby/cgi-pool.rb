#!/usr/bin/ruby

$VERBOSE = true

require 'cgi'
require 'stringio'
require 'Pool'
include Plasma

def json_quote(x)
  x.gsub(/[\000-\037\"\\]/) do |c|
    if c.respond_to? :ord
      n = c.ord
    else
      n = c[0]
    end
    "\\u%04x" % n
  end
end

class Jsonifier
  public

  def initialize(space, newline)
    @sp = space
    @sp2 = space * 2
    @nl = newline
  end

  private

  def slaw_to_json_helper(s, out, indent)
    typ = s.type_tag.to_s
    case typ
    when ""
      out << indent << 'null'
    when "protein"
      out << indent << "{#{@nl}"
      out << indent << @sp2 << '"descrips":' << @nl
      slaw_to_json_helper(s.descrips, out, indent + @sp2)
      out << ",#{@nl}"
      out << indent << @sp2 << '"ingests":' << @nl
      slaw_to_json_helper(s.ingests, out, indent + @sp2)
      out << @nl
      out << indent << "}"
    when "cons"
      out << indent << "[#{@nl}"
      slaw_to_json_helper(s.car, out, indent + @sp2)
      out << ",#{@nl}"
      slaw_to_json_helper(s.cdr, out, indent + @sp2)
      out << @nl
      out << indent << "]"
    when "list"
      out << indent << "[#{@nl}"
      1.upto(s.count) do |i|
        slaw_to_json_helper(s.nth_slaw(i-1), out, indent + @sp2)
        out << ((i == s.count) ? "" : ",") << @nl
      end
      out << indent << "]"
    when "map"
      out << indent << "{#{@nl}"
      1.upto(s.count) do |i|
        slaw_to_json_helper(s.nth_slaw(i-1).car, out, indent + @sp2)
        out << ":#{@nl}"
        slaw_to_json_helper(s.nth_slaw(i-1).cdr, out, indent + @sp2)
        out << ((i == s.count) ? "" : ",") << @nl
      end
      out << indent << "}"
    when "string"
      out << indent << '"' << json_quote(s.emit) << '"'
    when "boolean"
      out << indent << s.emit
    when /^([ui]n|floa)t\d+$/
      out << indent << s.emit
    when /^v\d([ui]n|floa)t\d+$/
      out << indent << "[" << s.emit.join(",#{@sp}") << "]"
    when /^([ui]n|floa)(t\d+)_array$/
      out << indent << "[" << s.emit.join(",#{@sp}") << "]"
    when /^(v\d)([ui]n|floa)(t\d+)_array$/
      out << indent << "[" <<
        s.emit.map{|x| "[" + x.join(",#{@sp}") + "]"}.join(", ") << "]"
    else
      out << indent << '"' << typ << '"'
    end
  end

  public

  def slaw_to_json(s)
    str = StringIO.new("", "w")
    slaw_to_json_helper(s, str, "")
    str << @nl
    str.string
  end
end

# An IPv6 address has to go in brackets in a pool URI, so that its
# colons can't be mistaken for the one before the port.
def uri_host(h)
  (h.include?(":") && !h.start_with?("[")) ? "[#{h}]" : h
end

cgi = CGI.new
path = ENV["PATH_INFO"].split(/\//).find_all {|s| s.size > 0}
case path.size
when 2
  pool = path[0]
  idx = path[1]
when 3
  pool = "tcp://#{uri_host(path[0])}/#{path[1]}"
  idx = path[2]
when 4
  pool = "tcp://#{uri_host(path[0])}:#{path[1]}/#{path[2]}"
  idx = path[3]
else
  cgi.out("text/plain") { "bad path!" }
  exit
end

# mime_type = "text/x-yaml"
# mime_type = "application/json"
mime_type = "text/plain"

if ARGV.length == 0
  jsonifier = Jsonifier.new(" ", "\n")
else
  jsonifier = Jsonifier.new("", "")
end

cgi.out("type" => mime_type, "charset" => "utf-8") do
  begin
    h = Pool.participate(pool)
    if (idx == "index")
      '{"oldest": ' + h.oldest_index.to_s + ', "newest": ' +
        h.newest_index.to_s + '}' + "\n"
    elsif (idx == "deposit")
      something = cgi['protein']
      if something.respond_to? :read
        something = something.read
      end
      prot = Slaw.from_yaml("--- !<tag:oblong.com,2009:slaw/protein>\n" +
                            something)
      idx = h.deposit(prot)
      '{"index": ' + idx.to_s + "}\n"
    else
      jsonifier.slaw_to_json(h.nth(idx.to_i))
    end
  rescue PoolOperationError
    '{"error": "' + json_quote($!.to_s) + '"}' + "\n"
  end
end
