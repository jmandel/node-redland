rdf = require("./rdf_binding");

var toparse = "<?xml version=\"1.0\"?>\
<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\
     xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\
  <rdf:Description rdf:about=\"http://www.dajobe.org/\">\
    <dc:title>Dave Beckett's Home Page</dc:title>\
    <dc:creator>Dave Beckett</dc:creator>\
    <dc:description>The generic home page of Dave Beckett.</dc:description>\
  </rdf:Description> \
</rdf:RDF>";

console.log("required");
var st = new Date().getTime();
count = 0;
for (var i =0; i < 1000; i++) {
    (function() {
	var m = new rdf.Model();
	m.parseFromString(toparse, function(res) {
	    m.serialize("application/turtle", function(r) {
		if (++count==1000)  {
		    console.log(r);
		    var et = new Date().getTime() - st;
		    console.log("Done! " + et);
		    
		}
	    });
	});	     
    }());
}