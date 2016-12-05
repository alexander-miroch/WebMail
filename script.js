var checked = 0

function get_verb(c) {
	var f,varq;

	if (c != 11)
		f = document.forms['msg'].copyto
	else f = document.forms['msg'].markas
	for (i = 1; i < f.length; ++i) {
		if (f[i].selected) {
			varq = f[i].text
			break
		}
	}
	switch (c) {
		case 1:
			return "Удалить сообщения?"
		case 2:
			if (typeof(varq) == "undefined") return 0;
			return "Копировать сообщение(я) в папку" + varq + " ?"
		case 5:
			if (typeof(varq) == "undefined") return 0;
			return "Переместить cообщение(я) в папку" + varq + " ?"
		case 11:
			if (typeof(varq) == "undefined") return 0;
			return "Пометить сообщения " + varq + " ?"
		default:
			return 0
	}
}

function goahead(val) {
	var form,verb;

	verb = get_verb(val)
	if (!verb) return
	if (!confirm(verb)) return 

	form = document.forms['msg']
	form.todo.value = val
	form.submit()
}

function c_empty() {
	return confirm("Текущая папка будет очищена. Восстановить сообщения будет невозможно. Продолжить?")
}

function c_drop() {
	return confirm("Текущая папка будет удалена. Восстановить сообщения будет невозможно. Продолжить?")
}

function hl(e,v) {
	bg = v ? "#605d4c" : "#808285"

//	e.style.background = bg
}

function go_ab_ahead(val) {
	var form;

	if (!confirm("Вы уверены?")) return 

	form = document.forms['abook_f']
	form.todo.value = val
	form.submit()
}

function f_hide(select) {
	var form

	form = document.forms['filter'].what_todo_folder;
	if (select.value == 3) {
		form.disabled = true
	} else form.disabled = false
	
}

function go_fi_ahead(val) {
	var form 

	if (!confirm("Вы уверены?")) return 

	form = document.forms['filter_f']
	form.todo.value = val
	form.submit()
}

function go_cmsg_ahead(val) {
	var form;

	form = document.forms['compose']
	form.todo.value = val
	form.submit()
}

function select_all(name) {
	var elts;
	var i;
	

	checked = checked ? 0 : 1
	elts = document.forms[name].elements;
	for (i = 1; i < elts.length; ++i) {
		elts[i].checked = checked
	}
}

function doreq() {
	var xmlHttp;
	try {  
		xmlHttp = new XMLHttpRequest();
	} catch (e) {  
		try {
			xmlHttp = new ActiveXObject("Msxml2.XMLHTTP");    
		} catch (e) {
			try {
				xmlHttp = new ActiveXObject("Microsoft.XMLHTTP");      
			} catch (e) {
				alert("Your browser does not support AJAX!"); 
				return false; 
			}
		}
	}

	xmlHttp.onreadystatechange = function() {
		if(xmlHttp.readyState==4) {
			document.myForm.time.value = xmlHttp.responseText;
		}
	}
	xmlHttp.open("GET","time.asp",true);
	xmlHttp.send(null);  
}

function f_sub(form) {
//	act = document.getElementById("pbar")
	act.style.visibility = 'visible'
	return true
}

function frame_go(iframe) {
	var doc = iframe.contentDocument;
	var dv

	if (!doc && iframe.contentWindow) doc = iframe.contentWindow.document;
	if (!doc) doc = window.frames[iframe.id].document;
	if (!doc) return null;

	act = document.getElementById("pbar")
	act.style.visibility = 'hidden'

	rv = doc.documentElement.getElementsByTagName("p")[0];
	dv = document.getElementById("dv")
	if (typeof(rv) != "undefined") dv.innerHTML = rv.innerHTML
}

function setup_parent() {
	var addr,len, i;
	var pr;
	var s = 0;

	if (parent.opener.closed) {
		alert('Системная ошибка')
		window.close()
	}
	pr = parent.opener.document.compose
	if (!pr) {
		alert('Системная ошибка')
		window.close()
	}

	return pr
}

function clearfrom() {
	var pr

	pr = setup_parent()
	pr.c_to.value = ""
}


function clear_cc() {
	var pr

	pr = setup_parent()
	pr.c_copy.value = ""
}

function add_cc() {
	var addr,len, i;
	var pr;
	var s = 0;

	pr = setup_parent()	
	addr = document.forms['addr'].addrs
	len = addr.options.length
	for (i = 0; i < len; ++i) {
		if (addr.options[i].selected) {
			
			if (pr.c_copy.value) 
				pr.c_copy.value += "," + addr.options[i].value
			else pr.c_copy.value = addr.options[i].value
		}
	}
	return
}

function addfrom() {
	var addr,len, i;
	var pr;
	var s = 0;

	pr = setup_parent()	
	addr = document.forms['addr'].addrs
	len = addr.options.length
	for (i = 0; i < len; ++i) {
		if (addr.options[i].selected) {
			if (s) {
				alert('Вы должны выбрать только один адрес')
				break
			}
			s = 1;
			pr.c_to.value = addr.options[i].value
		}
	}
	return
}


