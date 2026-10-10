"""Join bounded read-only native enemy pages before admitting movement."""
import math
import time

from .core import BotFault


def read_enemy_snapshot(read, script, *, clock=time.monotonic, max_seconds=3.):
    cursor, identity, normal, soldiers = 0, None, None, []
    unloaded, inspected, pages = 0, 0, []
    names = set()
    started = clock()
    for _ in range(128):  # 8192 native definitions, 64 per page.
        if clock()-started>max_seconds:
            raise BotFault('Complete enemy snapshot exceeded its bounded read deadline')
        page, timing = read(f'local MGS5VR_NAV_QUERY_CURSOR={cursor}\n'+script)
        page_identity = (page.get('mission'), page.get('location'),
                         page.get('definition_identity'), page.get('definitions_total'))
        if identity is None:
            identity, normal = page_identity, page.get('normal_life')
            if (any(type(v) is not int for v in identity) or not 0<=identity[3]<=8192
                    or not isinstance(normal, str)):
                raise BotFault('Native enemy page identity is unavailable')
        if page_identity!=identity or page.get('normal_life')!=normal:
            raise BotFault('Native enemy world/definitions changed between pages')
        if page.get('page_cursor')!=cursor or page.get('truncated') is not False or page.get('unavailable_loaded')!=0:
            raise BotFault('Loaded enemy telemetry is incomplete; movement released')
        count, absent = page.get('definitions_inspected'), page.get('unloaded_count')
        rows = page.get('soldiers')
        if (type(count) is not int or not 0<=count<=64 or type(absent) is not int
                or absent<0 or not isinstance(rows,list) or len(rows)+absent!=count):
            raise BotFault('Native enemy page accounting is invalid')
        for row in rows:
            point = row.get('position')
            if (not isinstance(row.get('name'),str) or row['name'] in names
                    or type(row.get('id')) is not int or not 0<row['id']<65535
                    or not isinstance(row.get('life'),str)
                    or not isinstance(point,list) or len(point)!=3
                    or any(type(v) not in (int,float) or not math.isfinite(v) for v in point)):
                raise BotFault('Native enemy page contains an ambiguous actor or position')
            names.add(row['name']); soldiers.append(row)
        inspected+=count;unloaded+=absent;pages.append(timing)
        next_cursor=page.get('next_cursor')
        if next_cursor is None:
            if cursor+count!=identity[3] or inspected!=identity[3]:
                raise BotFault('Native enemy pagination did not cover every definition')
            return {'mission':identity[0], 'location':identity[1], 'normal_life':normal,
                    'definition_identity':identity[2], 'definitions_inspected':inspected,
                    'unloaded_count':unloaded, 'unavailable_loaded':0, 'truncated':False,
                    'soldiers':soldiers, 'pages':pages, 'read_seconds':clock()-started}
        if type(next_cursor) is not int or next_cursor!=cursor+count or not cursor<next_cursor<=identity[3]:
            raise BotFault('Native enemy page cursor failed to advance exactly')
        cursor=next_cursor
    raise BotFault('Native enemy pagination exceeded its page limit')
