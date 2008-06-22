class Tweet < ActiveRecord::Base
  belongs_to :profile

  require 'uri'

  def self.url2link_of_body( html_string )
    URI.extract(html_string, "http").each{|url|
      html_string.gsub!(url,"<a href='#{url}' target='_blank'>#{url}</a>")
    }
    html_string
  end

end
