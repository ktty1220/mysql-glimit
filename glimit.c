// /etc/init.d/mysqld: env LD_LIBRARY_PATH=/usr/local/lib/mysql \ on mysqld_safe ...
/** 
 * @file  glimit.c
 * @brief グループ毎に件数を指定して抽出するMySQL UDF
 *
 * @version 1.0
 * @author ktty1220 <ktty1220@gmail.com>
 * @copyright Copyright (C) 2013 by ktty1220. All rights reserved.
 * @see http://cstl.sourceforge.jp/
 *
 * License http://opensource.org/licenses/BSD-3-Clause BSD-3-Clause License.
 */
#include <stdlib.h>
#include <string.h>
#include <my_global.h>
#include <mysql.h>
#include <cstl/map.h>

#define SEPARATOR '\1'

// グループカウント用のマップ定義
CSTL_MAP_INTERFACE(groupMap, char *, int)
CSTL_MAP_IMPLEMENT(groupMap, char *, int, strcmp)

/** SQL関数実行前初期処理
 * @param initid 関数間のデータ受け渡し用構造体
 * @param args 関数呼び出し時の引数
 * @param message エラーメッセージ格納用ポインタ
 * @retval 0 正常終了
 * @retval 1 エラー
 */
my_bool glimit_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  // 呼び出し時に渡されたパラメータの数が2未満の場合はエラー
  if (args->arg_count < 2) {
    strcpy(message, "glimit(int, string, ...) requires 1 integer and at least 1 string!");
    return 1;
  }

  // パラメータの型チェック 1番目がintでない場合はエラー
  if (args->arg_type[0] != INT_RESULT) {
    strcpy(message, "glimit() requires a integer at first argument!");
    return 1;
  }

  // グループカウント用のマップハンドルを開いてinitidのフリー領域に保持する
  groupMap *map = groupMap_new();
  initid->ptr = (void *) map;

  // 2番目以降のパラメータは全て文字列扱いとする
  int i;
  for (i = 1; i < args->arg_count; i++) {
    args->arg_type[i] = STRING_RESULT;
  }

  return 0;
}

/** SQL関数終了時処理
 * @param initid 関数間のデータ受け渡し用構造体
 */
void glimit_deinit(UDF_INIT *initid) {
  // glimit_init()で開いたマップハンドルをクローズし、メモリを開放する
  if (initid->ptr) {
    groupMap *map = (groupMap *) initid->ptr;
    // 各keyの文字列のメモリを開放
    groupMapIterator itr;
    for (itr = groupMap_begin(map); itr != groupMap_end(map); itr = groupMap_next(itr)) {
      free(*groupMap_key(itr));
    }
    groupMap_delete(map);
    map = NULL;
  }
}

/** SQL関数実行(各行毎に呼ばれる)
 * @param initid 関数間のデータ受け渡し用構造体
 * @param args 関数呼び出し時の引数
 * @param is_null NULLを返すフラグ(1バイト?)
 * @param error エラーを返すフラグ(1バイト)
 * @retval 0 抽出対象外
 * @retval 1 抽出対象
 */
long long glimit(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
  is_null = 0;

  // 1番目のパラメータは各グループ毎のlimit数
  int limit = *(int *) args->args[0];

  // 2番目以降のパラメータの文字列長の合計を算出
  int i, len = 0, ret = 0;
  for (i = 1; i < args->arg_count; i++) {
    // パラメータにNULLが混じっている場合は無条件で抽出対象外
    if (args->args[i] == NULL) { return 0; }
    len += args->lengths[i];
  }

  // keyに合計文字列長 + 区切り文字分のメモリを確保
  char *key = (char *) malloc(len + args->arg_count);
  memset(key, 0, len + args->arg_count);

  // keyに2番目以降ののパラメータの文字列を\x01区切りで連結コピーしていく
  int klen = 0;
  for (i = 1; i < args->arg_count; i++) {
    strncpy(key + klen, args->args[i], args->lengths[i]);
    klen += args->lengths[i];
    if (i < args->arg_count - 1) key[klen++] = SEPARATOR;
  }
  key[klen] = '\0';

  // glimit_init()で開いたマップハンドルをkeyで検索し、該当するvalueのカウントを増加する
  // valueのカウントがlimitに達するまでは1を、limitに達したら0を返す
  groupMap *map =(groupMap *) initid->ptr;
  if (!map) { 
    *error = 1;
  } else {
    int success;
    groupMapIterator itr = groupMap_insert(map, key, 1, &success);
    if (success != 0) {
      // 新規挿入
      ret = 1;
    } else {
      // keyが存在
      if (*groupMap_value(itr) < limit) {
        ret = 1;
        *groupMap_value(itr) += 1;
      }
      // keyのメモリを開放(新規挿入の場合はそのポインタがマップに保存されるようなので既存の場合のみメモリを開放する)
      free(key);
    }
  }

  return ret;
}
