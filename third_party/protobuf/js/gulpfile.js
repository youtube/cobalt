var gulp = require('gulp');
var execFile = require('child_process').execFile;
var glob = require('glob');

function exec(command, cb) {
  execFile('sh', ['-c', command], cb);
}

var protoc = process.env.PROTOC || '../src/protoc';

gulp.task('genproto_closure', function (cb) {
  exec(protoc + ' --js_out=library=testproto_libs,binary:.  -I ../src -I . *.proto ../src/google/protobuf/descriptor.proto',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('genproto_commonjs', function (cb) {
  exec('mkdir -p commonjs_out && ' + protoc + ' --js_out=import_style=commonjs,binary:commonjs_out -I ../src -I commonjs -I . *.proto commonjs/test*/*.proto ../src/google/protobuf/descriptor.proto',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('dist', function (cb) {
  // TODO(haberman): minify this more aggressively.
  // Will require proper externs/exports.
  exec('./node_modules/google-closure-library/closure/bin/calcdeps.py -i message.js -i binary/reader.js -i binary/writer.js -i commonjs/export.js -p . -p node_modules/google-closure-library/closure -o compiled --compiler_jar node_modules/google-closure-compiler/compiler.jar > google-protobuf.js',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('commonjs_asserts', function (cb) {
  exec('mkdir -p commonjs_out/test_node_modules && ./node_modules/google-closure-library/closure/bin/calcdeps.py -i commonjs/export_asserts.js -p . -p node_modules/google-closure-library/closure -o compiled --compiler_jar node_modules/google-closure-compiler/compiler.jar > commonjs_out/test_node_modules/closure_asserts_commonjs.js',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('commonjs_testdeps', function (cb) {
  exec('mkdir -p commonjs_out/test_node_modules && ./node_modules/google-closure-library/closure/bin/calcdeps.py -i commonjs/export_testdeps.js -p . -p node_modules/google-closure-library/closure -o compiled --compiler_jar node_modules/google-closure-compiler/compiler.jar > commonjs_out/test_node_modules/testdeps_commonjs.js',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('make_commonjs_out', ['dist', 'genproto_commonjs', 'commonjs_asserts', 'commonjs_testdeps'], function (cb) {
  // TODO(haberman): minify this more aggressively.
  // Will require proper externs/exports.
  var cmd = "mkdir -p commonjs_out/binary && mkdir -p commonjs_out/test_node_modules && ";
  function addTestFile(file) {
    cmd += 'node commonjs/rewrite_tests_for_commonjs.js < ' + file +
           ' > commonjs_out/' + file + '&& ';
  }

  glob.sync('*_test.js').forEach(addTestFile);
  glob.sync('binary/*_test.js').forEach(addTestFile);

  exec(cmd +
       'cp commonjs/jasmine.json commonjs_out/jasmine.json && ' +
       'cp google-protobuf.js commonjs_out/test_node_modules && ' +
       'cp commonjs/import_test.js commonjs_out/import_test.js',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('deps', ['genproto_closure'], function (cb) {
  exec('./node_modules/google-closure-library/closure/bin/build/depswriter.py *.js binary/*.js > deps.js',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('test_closure', ['genproto_closure', 'deps'], function (cb) {
  exec('JASMINE_CONFIG_PATH=jasmine.json ./node_modules/.bin/jasmine',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('test_commonjs', ['make_commonjs_out'], function (cb) {
  exec('cd commonjs_out && JASMINE_CONFIG_PATH=jasmine.json NODE_PATH=test_node_modules ../node_modules/.bin/jasmine',
       function (err, stdout, stderr) {
    console.log(stdout);
    console.log(stderr);
    cb(err);
  });
});

gulp.task('test', ['test_closure', 'test_commonjs'], function(cb) {
  cb();
});
