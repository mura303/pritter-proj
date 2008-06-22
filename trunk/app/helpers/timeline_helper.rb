module TimelineHelper

  def plural(n)
    if n == 1
      return ''
    else
      return 's'
    end
  end    

  def timediff_str( target )

    return target.strftime( "%I:%M %p %B %d, %Y" ) if Time.now.day != target.day # always absolute for another day

    diff = Time.now - target

    if diff < 1.minute
      n = diff.to_i
      return "#{n} second#{plural(n)} ago"
    elsif diff < 1.hour
      n = (diff/60).to_i
      return "#{n} minute#{plural(n)} ago"
    elsif diff < 1.day
      n = (diff/3600).to_i
      return "about #{n} hour#{plural(n)} ago"
    else
      return target.strftime( "%I:%M %p %B %d, %Y" )
    end
  end

  def about_pritter
    return "Twitter clone for private networks"
  end

end
