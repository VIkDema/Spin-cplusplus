mtype = {LOCK, UNLOCK};
mtype turn = LOCK;
bool critical_section = false;

bool P_critical_section = false;
bool Q_critical_section = false;

active proctype P() {
    do
    :: turn == LOCK ->
        turn = UNLOCK;
        atomic {
            // mutex
			printf("P");
            critical_section = true;
            P_critical_section = true; // flag
        }
        turn = LOCK;
        P_critical_section = false; // flag
    od
}

active proctype Q() {
    do
    :: turn == UNLOCK ->
        turn = LOCK;
        atomic {
            // free mutes
			printf("Q");
            critical_section = false;
            Q_critical_section = true; // flag
        }
        turn = UNLOCK;
        Q_critical_section = false; // flag
    od
}

ltl { [](! (P_critical_section || Q_critical_section)) }
