var f_disp = 1
var f_open = 2
var f_curr = 4

function Menu() {
	this.imgdir = '/images/tree'
	
	this.imgBlank       = 'blank.png'
	this.imgFolder      = 'folder.png'
	this.imgFolderOpen  = 'folderopen.png'

	this.imgLine        = 'line.png'
	this.imgJoin        = 'join.png'
	this.imgJoinBottom  = 'joinbottom.png'
	this.imgPlus        = 'plus.png'
	this.imgPlusBottom  = 'plusbottom.png'
	this.imgPlusOnly    = 'plusonly.png'
	this.imgMinus       = 'minus.png'
	this.imgMinusBottom = 'minusbottom.png'
	this.imgMinusOnly   = 'minusonly.png'
	this.imgNullOnly    = 'nullonly.png'
	this.imgLeaf        = 'leaf.png'

	this.saved = '' 
	this.out = ''
	this.root = new Array()
	this.total = 0
}

Menu.prototype.add = function(pid,id,name,status) {
	var tp,p,idx;
	var i

	if (!pid) {
		this.setelem(this.root,id,name,status)
		return
	}
	
	p = this.traverse(this.root,pid)
	if (!p) return
	this.set_childelem(p,id,name,status)


}

Menu.prototype.set_childelem = function(p,id,name,status) {
	var l

	if (typeof(p.childs) == "undefined") {
		p.childs = new Array()
	}

	this.setelem(p.childs,id,name,status)
}

Menu.prototype.setelem = function(p,id,name,status) {
	var l

	l = p.length
	p[l] = new Array()
	p[l]['id'] = id
	p[l]['name'] = name
	p[l]['status'] = status
	this.total++;
}

Menu.prototype.setdisp = function(id,val) {
	var doc

	doc = document.getElementById("b_"+id)
	doc.style.display = val
}

/*
1 - open
0 - closed
*/
Menu.prototype.switch_to = function(e) {
	var i,p,doc
	var disp
	var isopen

	p = e.childs
	if (typeof(p) == "undefined") return 0;
	isopen = (e['status'] & f_open) ? 1 : 0

	for (i = 0; i < p.length; ++i) {
		if (isopen) {
			/* CLOSE ALL */
			if (typeof(p[i].childs) != "undefined") {
				p[i]['status'] |= f_open
				this.switch_to(p[i])
			}
			p[i]['status'] &= ~f_disp
			p[i]['status'] &= ~f_open
			this.setdisp(p[i]['id'],'none')
		} else {
			if (typeof(p[i].childs) != "undefined") {
				/* OPEN FIRST CHILDS */
				p[i]['status'] &= ~f_open
				this.setplus(p[i],1)
			} 
			p[i]['status'] |= f_disp
			this.setdisp(p[i]['id'],'block')
		}
	}

	if (isopen) e['status'] &= ~f_open
	else e['status'] |= f_open

	return 0
}

Menu.prototype.setplus = function(e,plus) {
	var img
	var p
	var s1,s2

	s1 = "plus"
	s2 = "minus"

	img = document.getElementById("i_"+e['id'])
	if (!img) return
	p = img.src

	if (!plus) {
		if (e['status'] & f_open) p = p.replace(s2,s1)
		else p = p.replace(s1,s2)
	} else p = p.replace(s2,s1)
	img.src = p
}

Menu.prototype.toggle = function(id) {
	var e

	e = this.traverse(this.root,id)
	if (!e) return

	this.setplus(e,0)
	//document.write("i="+img)
	this.switch_to(e)
	this.save_state()
}


Menu.prototype.save_state = function() {
	this.getcookie()
	this.setcookie()
}

Menu.prototype.getcookie = function() {
	var oc,dc
	var b,e,ncook = 0

	oc = document.cookie
//	document.write("oc="+oc+"-END")
	b = oc.indexOf('; wmail_state=')
	if (b < 0) {
		b = oc.indexOf('wmail_state=')
		if (b < 0) ncook  = 1
	} else b += 2
	
	if (!ncook) {
		e = oc.indexOf(';',b)
		if (e < 0) e = oc.length
		dc = oc.substr(b+12,e-b-12)
	} 
	return dc
}

Menu.prototype.setcookie = function() {
	var cook

	this.saved = ''
	this.addc(this.root)
	cook = "wmail_state=" + this.saved + "; "
	//document.write("<br>will set this="+cook+"<br>");
	document.cookie = cook
	
}

Menu.prototype.addc = function(e) {
	var i,p
	var prob_pid

	for (i = 0; i < e.length; ++i) {
		this.saved += e[i]['status']
		if (typeof(e[i].childs) != "undefined") {
			this.addc(e[i].childs)
		}
	}
}


Menu.prototype.disp = function(e) {
	if (e['status'] & f_disp) return "block";
	else return "none";
}

Menu.prototype.gethtml = function(e,nest,islast,lp) {
	var disp
	var i
	var ss
	var cnest

	bg = (e['status'] & f_curr) ? "url(/images/ar1.jpg); background-repeat: no-repeat; background-position: bottom left" : "white";
	disp = this.disp(e)
	this.out += "<div style='position:relative; height:20px; width:200px; margin:0px;padding:0px; \
		 display: " + disp + "; cursor:pointer; background: "+bg+";' \
		 id='b_"+e['id']+"'>"


	for (i = 0; i < nest; ++i) {
	//	this.out += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
	}

	i = 0
	
	while (i < nest) {
		if (!i) {
			this.out += "<img class=x border=0 src='"+this.imgdir+"/"+this.imgBlank+"'>"
			i++
			continue
		}
		
		
	
		if (!lp)
			this.out += "<img class=x border=0 src='"+this.imgdir+"/"+this.imgLine+"'>"
		else {
			if (i == nest - 1)
			this.out += "<img class=x border=0 src='"+this.imgdir+"/"+this.imgBlank+"'>"
			else
			this.out += "<img class=x border=0 src='"+this.imgdir+"/"+this.imgLine+"'>"
		}
		++i;
	}

	this.out += "<img class=x vspace=\"0\" id='i_"+e['id']+"' onclick='M.toggle("+e['id']+")' src='" + this.imgdir + "/"
	if (typeof(e.childs) != "undefined") {
		if (nest) {
			if (e['status'] & f_open) {
				if (islast) this.out += this.imgMinusBottom
				else this.out += this.imgMinus
			} else {
				if (islast) this.out += this.imgPlusBottom
				else this.out += this.imgPlus
			}
		} else {
			if (e['status'] & f_open) {
				this.out += this.imgMinusOnly
			} else {
				this.out += this.imgPlusOnly
			}
		}
	} else {
		if (nest) {
			if (islast) this.out += this.imgJoinBottom
			else this.out += this.imgJoin
		} else {
			this.out += this.imgNullOnly
		}
	}
	this.out += "'>"
	/*blyaaa...*/
	//this.out += "<img src='/images/folder.png'>&nbsp;"

	//this.out += "d"
	this.out += e['name'] 
	this.out += "</div>"
}

Menu.prototype.show_all = function(e,nest) {
	var doc
	var cook

	cook = this.getcookie()
	if (cook && typeof(cook) != "undefined") {
		if (cook.length == this.total) this.saved = cook
	}

	this.show(e,nest,1)
	doc = document.getElementById("ttdiv")
	doc.innerHTML = this.out
}

Menu.prototype.show = function(e,nest,lp) {
	var i,islast,add = 0 
	
	for (i = 0; i < e.length; ++i) {
		islast = (i == e.length - 1);
		if (this.saved)	{
			if (e[i]['status'] & f_curr) add = 1
			e[i]['status'] = this.saved.charAt(e[i]['id']-1)
			if (add) e[i]['status'] |= f_curr
			else e[i]['status'] &= ~f_curr
			add = 0
		}
		this.gethtml(e[i],nest,islast,lp)
		if (typeof(e[i].childs) != "undefined") {
			++nest
			this.show(e[i].childs,nest,islast)
			--nest
		}
	}
}

Menu.prototype.traverse = function(e,pid) {
	var i,p
	var prob_pid

	for (i = 0; i < e.length; ++i) {
		if (typeof(e[i].childs) != "undefined") {
			prob_pid = this.traverse(e[i].childs,pid)
			if (prob_pid) return prob_pid
		}
		if (e[i]['id'] == pid) return e[i]
	}
	return 0
}


