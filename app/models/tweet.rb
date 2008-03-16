class Tweet < ActiveRecord::Base
  belongs_to :profile

  require 'uri'

  def self.url2link_of_body( body )
    html_string = CGI::escapeHTML(body)
    URI.extract(html_string).each{|url|
      html_string.gsub!(url,"<a href='#{url}' target='_blank'>#{url}</a>")
    }
    html_string
  end

end
